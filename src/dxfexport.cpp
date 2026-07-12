#include "dxfexport.h"

#include <QFile>
#include <QTextStream>
#include <QtMath>
#include "scale.h"

/*
 * Minimal DXF R12 (AC1009) ASCII writer.
 * All coordinates are output in millimetres (internal units / PRECISIONSCALE).
 * Tracks and oval pads are written as 2-vertex POLYLINEs carrying the trace
 * width; circle pads and drills as CIRCLEs; rectangle pads as closed
 * POLYLINEs. Polygon and macro pads use the same approximations as the
 * on-screen renderer (circle / rectangle).
 */

namespace {

constexpr int kPrec = 6; // 1 internal unit = 1 nm, so 6 decimals is exact in mm

QString num(double v)
{
    return QString::number(v, 'f', kPrec);
}

struct DxfLayer
{
    QString name;
    int color; // AutoCAD color index
};

class DxfWriter
{
public:
    DxfWriter(QFile *file, bool flipX)
        : m_out(file), m_xSign(flipX ? -1.0 : 1.0) {}

    void begin(const QList<DxfLayer> &layers)
    {
        // HEADER
        m_out << "0\nSECTION\n2\nHEADER\n";
        m_out << "9\n$ACADVER\n1\nAC1009\n";
        m_out << "9\n$INSUNITS\n70\n4\n"; // 4 = millimetres
        m_out << "0\nENDSEC\n";

        // TABLES: layer definitions
        m_out << "0\nSECTION\n2\nTABLES\n";
        m_out << "0\nTABLE\n2\nLAYER\n70\n" << layers.size() << "\n";
        for (const DxfLayer &l : layers)
        {
            m_out << "0\nLAYER\n2\n" << l.name << "\n70\n0\n"
                  << "62\n" << l.color << "\n6\nCONTINUOUS\n";
        }
        m_out << "0\nENDTAB\n0\nENDSEC\n";

        m_out << "0\nSECTION\n2\nENTITIES\n";
    }

    void end()
    {
        m_out << "0\nENDSEC\n0\nEOF\n";
    }

    // All positions below are in internal units (nm); widths/radii too.
    void line(const QString &layer, double x1, double y1, double x2, double y2)
    {
        m_out << "0\nLINE\n8\n" << layer << "\n"
              << "10\n" << num(mmX(x1)) << "\n20\n" << num(mmY(y1)) << "\n"
              << "11\n" << num(mmX(x2)) << "\n21\n" << num(mmY(y2)) << "\n";
    }

    void circle(const QString &layer, double cx, double cy, double r)
    {
        m_out << "0\nCIRCLE\n8\n" << layer << "\n"
              << "10\n" << num(mmX(cx)) << "\n20\n" << num(mmY(cy)) << "\n"
              << "40\n" << num(r / PRECISIONSCALE) << "\n";
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

        double w = width / PRECISIONSCALE;
        m_out << "0\nPOLYLINE\n8\n" << layer << "\n66\n1\n70\n0\n"
              << "40\n" << num(w) << "\n41\n" << num(w) << "\n";
        vertex(layer, x1, y1);
        vertex(layer, x2, y2);
        m_out << "0\nSEQEND\n";
    }

    void closedPolyline(const QString &layer, const QList<QPointF> &pts)
    {
        m_out << "0\nPOLYLINE\n8\n" << layer << "\n66\n1\n70\n1\n";
        for (const QPointF &p : pts)
            vertex(layer, p.x(), p.y());
        m_out << "0\nSEQEND\n";
    }

private:
    void vertex(const QString &layer, double x, double y)
    {
        m_out << "0\nVERTEX\n8\n" << layer << "\n"
              << "10\n" << num(mmX(x)) << "\n20\n" << num(mmY(y)) << "\n";
    }

    double mmX(double x) const { return x * m_xSign / PRECISIONSCALE; }
    double mmY(double y) const { return y / PRECISIONSCALE; }

    QTextStream m_out;
    double m_xSign;
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

    bool hasOutline = outline && !outline->tracksList.isEmpty();

    DxfWriter dxf(&file, flipX);
    QList<DxfLayer> layers { { layerName, 1 } };
    if (hasOutline)
        layers << DxfLayer{ "OUTLINE", 3 };
    dxf.begin(layers);

    for (const Track &t : g.tracksList)
        dxf.wideSegment(layerName, t.pointstart.x(), t.pointstart.y(),
                        t.pointend.x(), t.pointend.y(), t.width);

    for (const Pad &p : g.padsList)
        writePad(dxf, layerName, p);

    if (hasOutline)
    {
        for (const Track &t : outline->tracksList)
            dxf.line("OUTLINE", t.pointstart.x(), t.pointstart.y(),
                     t.pointend.x(), t.pointend.y());
    }

    dxf.end();
    file.close();
    return true;
}

bool DxfExport::writeDrillsOutline(const Gerber *outline,
                                   const ExcellonParser *excellon,
                                   const Preprocess *preprocess,
                                   const QString &filePath, QString &errorMsg,
                                   bool flipX)
{
    const QString kOutlineLayer = "OUTLINE";
    const QString kDrillsLayer  = "DRILLS";

    bool hasOutline = outline && !outline->tracksList.isEmpty();

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

    if (!hasOutline && holes.isEmpty())
    {
        errorMsg = "No outline or drill data to export.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file for writing: " + file.errorString();
        return false;
    }

    DxfWriter dxf(&file, flipX);
    dxf.begin({ { kOutlineLayer, 3 }, { kDrillsLayer, 2 } });

    if (hasOutline)
    {
        for (const Track &t : outline->tracksList)
            dxf.line(kOutlineLayer, t.pointstart.x(), t.pointstart.y(),
                     t.pointend.x(), t.pointend.y());
    }

    for (auto it = holes.cbegin(); it != holes.cend(); ++it)
    {
        double r = it.key() / 2.0;
        for (const QPoint &pt : it.value())
            dxf.circle(kDrillsLayer, pt.x(), pt.y(), r);
    }

    dxf.end();
    file.close();
    return true;
}
