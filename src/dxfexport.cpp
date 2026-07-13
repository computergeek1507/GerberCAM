#include "dxfexport.h"

#include <QFile>
#include <QTextStream>
#include <QtMath>
#include <cmath>
#include "scale.h"

/*
 * DXF R2000 (AC1015) ASCII writer.
 * The header/tables/blocks skeleton and the trailing OBJECTS section come
 * from res/dxf_skeleton_{prefix,suffix}.txt (generated with ezdxf, layers
 * TOP_COPPER/BOTTOM_COPPER/OUTLINE/DRILLS predefined, $HANDSEED patched
 * high); this writer only emits the ENTITIES section between them.
 * Copper artwork is exported the way KiCad plots DXF: every trace and pad is
 * turned into outline geometry (round-capped stroke contours, segmented
 * circles) and all shapes are merged with a Clipper union, so trace/pad
 * junctions form continuous contours. The merged outlines are written as
 * closed zero-width LWPOLYLINEs, which render identically in every
 * viewer/importer — polyline width attributes do not.
 * Drills are CIRCLEs; the board outline is thin LINEs.
 * All coordinates are output in millimetres (internal units / PRECISIONSCALE).
 */

namespace {

constexpr int kPrec = 6; // 1 internal unit = 1 nm, so 6 decimals is exact in mm

// Entity owner: the *Model_Space BLOCK_RECORD handle in the skeleton.
const char *kOwnerHandle = "17";
// First entity handle; the skeleton uses < 0x40 and $HANDSEED is 0xFFFFFF.
constexpr quint64 kFirstHandle = 0x100000;

QString num(double v)
{
    return QString::number(v, 'f', kPrec);
}

class DxfWriter
{
public:
    DxfWriter(QFile *file, bool flipX)
        : m_out(file), m_xSign(flipX ? -1.0 : 1.0) {}

    bool begin(QString &errorMsg)
    {
        QFile prefix(":/dxf_skeleton_prefix.txt");
        if (!prefix.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            errorMsg = "DXF skeleton resource missing.";
            return false;
        }
        m_out << prefix.readAll();
        return true;
    }

    void end()
    {
        QFile suffix(":/dxf_skeleton_suffix.txt");
        if (suffix.open(QIODevice::ReadOnly | QIODevice::Text))
            m_out << suffix.readAll();
    }

    // All positions below are in internal units (nm); widths/radii too.
    void line(const QString &layer, double x1, double y1, double x2, double y2)
    {
        entityHead("LINE", layer);
        m_out << "100\nAcDbLine\n"
              << " 10\n" << num(mmX(x1)) << "\n 20\n" << num(mmY(y1)) << "\n 30\n0.0\n"
              << " 11\n" << num(mmX(x2)) << "\n 21\n" << num(mmY(y2)) << "\n 31\n0.0\n";
    }

    void circle(const QString &layer, double cx, double cy, double r)
    {
        entityHead("CIRCLE", layer);
        m_out << "100\nAcDbCircle\n"
              << " 10\n" << num(mmX(cx)) << "\n 20\n" << num(mmY(cy)) << "\n 30\n0.0\n"
              << " 40\n" << num(r / PRECISIONSCALE) << "\n";
    }

    void closedPolyline(const QString &layer, const ClipperLib::Path &pts)
    {
        entityHead("LWPOLYLINE", layer);
        m_out << "100\nAcDbPolyline\n"
              << " 90\n" << pts.size() << "\n 70\n1\n";
        for (const ClipperLib::IntPoint &p : pts)
            m_out << " 10\n" << num(mmX(static_cast<double>(p.X)))
                  << "\n 20\n" << num(mmY(static_cast<double>(p.Y))) << "\n";
    }

private:
    void entityHead(const char *type, const QString &layer)
    {
        m_out << "  0\n" << type << "\n"
              << "  5\n" << QString::number(m_handle++, 16).toUpper() << "\n"
              << "330\n" << kOwnerHandle << "\n"
              << "100\nAcDbEntity\n"
              << "  8\n" << layer << "\n";
    }

    double mmX(double x) const { return x * m_xSign / PRECISIONSCALE; }
    double mmY(double y) const { return y / PRECISIONSCALE; }

    QTextStream m_out;
    double m_xSign;
    quint64 m_handle = kFirstHandle;
};

/*
 * Geometry builders: every solid shape becomes a closed polygon in internal
 * units, normalized to positive orientation so a NonZero Clipper union can
 * merge overlapping copper into continuous contours.
 */

void addPolygon(ClipperLib::Paths &solids, const QList<QPointF> &pts)
{
    ClipperLib::Path p;
    p.reserve(pts.size());
    for (const QPointF &pt : pts)
        p.push_back(ClipperLib::IntPoint(
            static_cast<ClipperLib::cInt>(std::llround(pt.x())),
            static_cast<ClipperLib::cInt>(std::llround(pt.y()))));
    if (p.size() < 3)
        return;
    if (!ClipperLib::Orientation(p))
        ClipperLib::ReversePath(p);
    solids.push_back(p);
}

QList<QPointF> circleOutline(double cx, double cy, double r, int segs = 32)
{
    constexpr double kPi = 3.14159265358979323846;
    QList<QPointF> pts;
    for (int i = 0; i < segs; ++i)
    {
        const double a = 2.0 * kPi * i / segs;
        pts << QPointF(cx + r * std::cos(a), cy + r * std::sin(a));
    }
    return pts;
}

// Outline contour of a round-capped stroke: two sides plus segmented caps.
QList<QPointF> strokeOutline(double x1, double y1, double x2, double y2,
                             double width)
{
    if (x1 == x2 && y1 == y2)
        return circleOutline(x1, y1, width / 2.0);

    constexpr int kCapSegs = 16;        // segments per semicircular cap
    constexpr double kPi = 3.14159265358979323846;

    const double dx = x2 - x1, dy = y2 - y1;
    const double len = std::hypot(dx, dy);
    const double nx = -dy / len, ny = dx / len; // left normal
    const double r = width / 2.0;
    const double a0 = std::atan2(ny, nx);       // angle of +normal

    QList<QPointF> pts;
    pts << QPointF(x1 + nx * r, y1 + ny * r)
        << QPointF(x2 + nx * r, y2 + ny * r);
    for (int i = 1; i < kCapSegs; ++i)          // cap around the end point
    {
        const double a = a0 - kPi * i / kCapSegs;
        pts << QPointF(x2 + r * std::cos(a), y2 + r * std::sin(a));
    }
    pts << QPointF(x2 - nx * r, y2 - ny * r)
        << QPointF(x1 - nx * r, y1 - ny * r);
    for (int i = 1; i < kCapSegs; ++i)          // cap around the start point
    {
        const double a = a0 + kPi - kPi * i / kCapSegs;
        pts << QPointF(x1 + r * std::cos(a), y1 + r * std::sin(a));
    }
    return pts;
}

void addPadGeometry(ClipperLib::Paths &solids, const Pad &pad)
{
    double cx = pad.point.x();
    double cy = pad.point.y();

    switch (pad.shape)
    {
    case PadShape::Circle:
        addPolygon(solids, circleOutline(cx, cy, pad.parameter[0] / 2.0));
        break;

    case PadShape::Oval:
    {
        // Same construction as the renderer: a wide stroke along the long axis.
        double p0 = pad.parameter[0];
        double p1 = pad.parameter[1];
        if (pad.angle == 0.0)
        {
            if (p0 >= p1) // horizontal
                addPolygon(solids, strokeOutline(cx - (p0 - p1) / 2.0, cy,
                                                 cx + (p0 - p1) / 2.0, cy, p1));
            else          // vertical
                addPolygon(solids, strokeOutline(cx, cy - (p1 - p0) / 2.0,
                                                 cx, cy + (p1 - p0) / 2.0, p0));
        }
        else
        {
            double half = (p0 - p1) / 2.0;
            double dx = half * qCos(pad.angle);
            double dy = half * qSin(pad.angle);
            addPolygon(solids, strokeOutline(cx - dx, cy - dy,
                                             cx + dx, cy + dy, p1));
        }
        break;
    }

    case PadShape::Rectangle:
    {
        double hw = pad.parameter[0] / 2.0;
        double hh = pad.parameter[1] / 2.0;
        double ca = qCos(pad.angle);
        double sa = qSin(pad.angle);
        QList<QPointF> corners;
        const double xs[4] = { -hw,  hw, hw, -hw };
        const double ys[4] = { -hh, -hh, hh,  hh };
        for (int i = 0; i < 4; ++i)
            corners << QPointF(cx + xs[i] * ca - ys[i] * sa,
                               cy + xs[i] * sa + ys[i] * ca);
        addPolygon(solids, corners);
        break;
    }

    case PadShape::Polygon:
        // Approximate as the circumscribing circle, like the parser does
        // for macro polygons.
        addPolygon(solids, circleOutline(cx, cy, pad.parameter[0] / 2.0));
        break;

    case PadShape::Macro:
        if (pad.parameterNum >= 2)
        {
            QList<QPointF> corners;
            double hw = pad.parameter[0] / 2.0;
            double hh = pad.parameter[1] / 2.0;
            corners << QPointF(cx - hw, cy - hh) << QPointF(cx + hw, cy - hh)
                    << QPointF(cx + hw, cy + hh) << QPointF(cx - hw, cy + hh);
            addPolygon(solids, corners);
        }
        else if (pad.parameterNum >= 1)
        {
            addPolygon(solids, circleOutline(cx, cy, pad.parameter[0] / 2.0));
        }
        break;
    }
}

} // namespace

bool DxfExport::writeCopper(const Gerber &g, const QString &layerName,
                            const QString &filePath, QString &errorMsg,
                            bool flipX, const Gerber *outline)
{
    if (g.tracksList.isEmpty() && g.padsList.isEmpty())
    {
        errorMsg = "No copper geometry to export.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file for writing: " + file.errorString();
        return false;
    }

    // Build all copper shapes as polygons and merge them so trace/pad
    // junctions become continuous contours (like KiCad's DXF plots).
    ClipperLib::Paths solids;
    for (const Track &t : g.tracksList)
        addPolygon(solids, strokeOutline(t.pointstart.x(), t.pointstart.y(),
                                         t.pointend.x(), t.pointend.y(),
                                         t.width));
    for (const Pad &p : g.padsList)
        addPadGeometry(solids, p);

    ClipperLib::Clipper clipper;
    clipper.AddPaths(solids, ClipperLib::ptSubject, true);
    ClipperLib::Paths merged;
    clipper.Execute(ClipperLib::ctUnion, merged,
                    ClipperLib::pftNonZero, ClipperLib::pftNonZero);

    DxfWriter dxf(&file, flipX);
    if (!dxf.begin(errorMsg))
        return false;

    for (const ClipperLib::Path &p : merged)
        dxf.closedPolyline(layerName, p);

    if (outline && !outline->tracksList.isEmpty())
    {
        for (const Track &t : outline->tracksList)
            dxf.line("OUTLINE", t.pointstart.x(), t.pointstart.y(),
                     t.pointend.x(), t.pointend.y());
    }

    dxf.end();
    file.close();
    return true;
}

bool DxfExport::writeOutline(const Gerber &outline,
                             const QString &filePath, QString &errorMsg,
                             bool flipX)
{
    if (outline.tracksList.isEmpty())
    {
        errorMsg = "No outline tracks to export.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file for writing: " + file.errorString();
        return false;
    }

    DxfWriter dxf(&file, flipX);
    if (!dxf.begin(errorMsg))
        return false;

    for (const Track &t : outline.tracksList)
        dxf.line("OUTLINE", t.pointstart.x(), t.pointstart.y(),
                 t.pointend.x(), t.pointend.y());

    dxf.end();
    file.close();
    return true;
}

bool DxfExport::writeDrills(const ExcellonParser *excellon,
                            const Preprocess *preprocess,
                            const QString &filePath, QString &errorMsg,
                            bool flipX)
{
    // Drill source: Excellon takes priority, else Gerber pad holes.
    QMap<qint64, QList<QPoint>> holes;
    if (excellon)
        holes = excellon->holesByDiameter();
    if (holes.isEmpty() && preprocess)
    {
        for (const Net &net : preprocess->netList)
            for (const Element &e : net.elements)
                if (e.elementType == ElementType::Pad && e.pad.hole > 0)
                    holes[e.pad.hole].append(e.pad.point);
    }

    if (holes.isEmpty())
    {
        errorMsg = "No drill data to export.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file for writing: " + file.errorString();
        return false;
    }

    DxfWriter dxf(&file, flipX);
    if (!dxf.begin(errorMsg))
        return false;

    for (auto it = holes.cbegin(); it != holes.cend(); ++it)
    {
        double r = it.key() / 2.0;
        for (const QPoint &pt : it.value())
            dxf.circle("DRILLS", pt.x(), pt.y(), r);
    }

    dxf.end();
    file.close();
    return true;
}
