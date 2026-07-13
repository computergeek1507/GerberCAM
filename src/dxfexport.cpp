#include "dxfexport.h"

#include <QFile>
#include <QTextStream>
#include <QtMath>
#include "scale.h"

/*
 * DXF R2000 (AC1015) ASCII writer.
 * The header/tables/blocks skeleton and the trailing OBJECTS section come
 * from res/dxf_skeleton_{prefix,suffix}.txt (generated with ezdxf, layers
 * TOP_COPPER/BOTTOM_COPPER/OUTLINE/DRILLS predefined, $HANDSEED patched
 * high); this writer only emits the ENTITIES section between them.
 * Traces and oval pads are LWPOLYLINEs with constant width — unlike the old
 * R12 POLYLINE encoding, these render in every common viewer/importer.
 * Circle pads and drills are CIRCLEs, rectangle pads closed LWPOLYLINEs;
 * polygon and macro pads use the same approximations as the on-screen
 * renderer (circle / rectangle).
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

    // A stroked segment with constant width (track, oval pad).
    void wideSegment(const QString &layer, double x1, double y1,
                     double x2, double y2, double width)
    {
        // A zero-length stroke with a round aperture is just a dot.
        if (x1 == x2 && y1 == y2)
        {
            if (width > 0.0)
                circle(layer, x1, y1, width / 2.0);
            return;
        }

        entityHead("LWPOLYLINE", layer);
        m_out << "100\nAcDbPolyline\n"
              << " 90\n2\n 70\n0\n"
              << " 43\n" << num(width / PRECISIONSCALE) << "\n"
              << " 10\n" << num(mmX(x1)) << "\n 20\n" << num(mmY(y1)) << "\n"
              << " 10\n" << num(mmX(x2)) << "\n 20\n" << num(mmY(y2)) << "\n";
    }

    void closedPolyline(const QString &layer, const QList<QPointF> &pts)
    {
        entityHead("LWPOLYLINE", layer);
        m_out << "100\nAcDbPolyline\n"
              << " 90\n" << pts.size() << "\n 70\n1\n";
        for (const QPointF &p : pts)
            m_out << " 10\n" << num(mmX(p.x())) << "\n 20\n" << num(mmY(p.y())) << "\n";
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

void writePad(DxfWriter &dxf, const QString &layer, const Pad &pad)
{
    double cx = pad.point.x();
    double cy = pad.point.y();

    switch (pad.shape)
    {
    case PadShape::Circle:
        dxf.circle(layer, cx, cy, pad.parameter[0] / 2.0);
        break;

    case PadShape::Oval:
    {
        // Same construction as the renderer: a wide stroke along the long axis.
        double p0 = pad.parameter[0];
        double p1 = pad.parameter[1];
        if (pad.angle == 0.0)
        {
            if (p0 >= p1) // horizontal
                dxf.wideSegment(layer, cx - (p0 - p1) / 2.0, cy,
                                cx + (p0 - p1) / 2.0, cy, p1);
            else          // vertical
                dxf.wideSegment(layer, cx, cy - (p1 - p0) / 2.0,
                                cx, cy + (p1 - p0) / 2.0, p0);
        }
        else
        {
            double half = (p0 - p1) / 2.0;
            double dx = half * qCos(pad.angle);
            double dy = half * qSin(pad.angle);
            dxf.wideSegment(layer, cx - dx, cy - dy, cx + dx, cy + dy, p1);
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
        dxf.closedPolyline(layer, corners);
        break;
    }

    case PadShape::Polygon:
        // Approximate as the circumscribing circle, like the parser does
        // for macro polygons.
        dxf.circle(layer, cx, cy, pad.parameter[0] / 2.0);
        break;

    case PadShape::Macro:
        if (pad.parameterNum >= 2)
        {
            QList<QPointF> corners;
            double hw = pad.parameter[0] / 2.0;
            double hh = pad.parameter[1] / 2.0;
            corners << QPointF(cx - hw, cy - hh) << QPointF(cx + hw, cy - hh)
                    << QPointF(cx + hw, cy + hh) << QPointF(cx - hw, cy + hh);
            dxf.closedPolyline(layer, corners);
        }
        else if (pad.parameterNum >= 1)
        {
            dxf.circle(layer, cx, cy, pad.parameter[0] / 2.0);
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

    DxfWriter dxf(&file, flipX);
    if (!dxf.begin(errorMsg))
        return false;

    for (const Track &t : g.tracksList)
        dxf.wideSegment(layerName, t.pointstart.x(), t.pointstart.y(),
                        t.pointend.x(), t.pointend.y(), t.width);

    for (const Pad &p : g.padsList)
        writePad(dxf, layerName, p);

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
