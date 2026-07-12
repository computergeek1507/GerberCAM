#include "svgexport.h"

#include <QFile>
#include <QTextStream>
#include <QtMath>
#include "scale.h"

/*
 * SVG 1.1 writer. All coordinates are output in millimetres (internal units /
 * PRECISIONSCALE); the document viewBox is in mm and width/height carry "mm"
 * units so the file opens at true scale. SVG's y axis points down, so y is
 * negated on output and the viewBox adjusted accordingly.
 *
 * Tracks and oval pads are round-capped strokes (an exact representation of
 * a round Gerber aperture); circle pads and drills are filled circles;
 * rectangle pads are filled polygons.
 */

namespace {

constexpr int kPrec = 4; // 4 decimals in mm = 0.1 µm resolution

QString num(double v)
{
    return QString::number(v, 'f', kPrec);
}

const QString kOutlineColor = "#00a000";
const QString kDrillColor   = "#404040";

class SvgWriter
{
public:
    explicit SvgWriter(bool flipX) : m_xSign(flipX ? -1.0 : 1.0) {}

    // All positions are in internal units (nm); widths/radii too.
    void line(const QString &color, double x1, double y1,
              double x2, double y2, double width)
    {
        double w = width / PRECISIONSCALE;
        grow(x1, y1, width / 2.0);
        grow(x2, y2, width / 2.0);
        m_body += "  <line x1=\"" + num(mmX(x1)) + "\" y1=\"" + num(mmY(y1))
                + "\" x2=\"" + num(mmX(x2)) + "\" y2=\"" + num(mmY(y2))
                + "\" stroke=\"" + color + "\" stroke-width=\"" + num(w)
                + "\" stroke-linecap=\"round\"/>\n";
    }

    void circle(const QString &color, double cx, double cy, double r)
    {
        grow(cx, cy, r);
        m_body += "  <circle cx=\"" + num(mmX(cx)) + "\" cy=\"" + num(mmY(cy))
                + "\" r=\"" + num(r / PRECISIONSCALE)
                + "\" fill=\"" + color + "\"/>\n";
    }

    // A stroked segment with constant width and round caps (track, oval pad).
    void wideSegment(const QString &color, double x1, double y1,
                     double x2, double y2, double width)
    {
        if (x1 == x2 && y1 == y2)
        {
            if (width > 0.0)
                circle(color, x1, y1, width / 2.0);
            return;
        }
        line(color, x1, y1, x2, y2, width);
    }

    void polygon(const QString &color, const QList<QPointF> &pts)
    {
        m_body += "  <polygon points=\"";
        for (int i = 0; i < pts.size(); ++i)
        {
            grow(pts[i].x(), pts[i].y(), 0.0);
            if (i) m_body += ' ';
            m_body += num(mmX(pts[i].x())) + ',' + num(mmY(pts[i].y()));
        }
        m_body += "\" fill=\"" + color + "\"/>\n";
    }

    bool save(const QString &filePath, QString &errorMsg) const
    {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            errorMsg = "Cannot open file for writing: " + file.errorString();
            return false;
        }

        // 1 mm margin around the geometry
        double minX = m_minX - 1.0, maxX = m_maxX + 1.0;
        double minY = m_minY - 1.0, maxY = m_maxY + 1.0;
        double w = maxX - minX, h = maxY - minY;

        QTextStream out(&file);
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""
            << num(w) << "mm\" height=\"" << num(h) << "mm\" viewBox=\""
            << num(minX) << ' ' << num(-maxY) << ' '
            << num(w) << ' ' << num(h) << "\">\n";
        out << m_body;
        out << "</svg>\n";
        file.close();
        return true;
    }

private:
    // mm output coords; SVG y axis points down, so negate y.
    double mmX(double x) const { return x * m_xSign / PRECISIONSCALE; }
    double mmY(double y) const { return -y / PRECISIONSCALE; }

    // Track the geometry extents in output mm coordinates (x already signed,
    // y kept board-up; save() converts to the SVG frame).
    void grow(double x, double y, double margin)
    {
        double mx = x * m_xSign / PRECISIONSCALE;
        double my = y / PRECISIONSCALE;
        double m  = margin / PRECISIONSCALE;
        m_minX = qMin(m_minX, mx - m);
        m_maxX = qMax(m_maxX, mx + m);
        m_minY = qMin(m_minY, my - m);
        m_maxY = qMax(m_maxY, my + m);
    }

    QString m_body;
    double m_xSign;
    double m_minX = 0.0, m_maxX = 0.0, m_minY = 0.0, m_maxY = 0.0;
};

void writePad(SvgWriter &svg, const QString &color, const Pad &pad)
{
    double cx = pad.point.x();
    double cy = pad.point.y();

    switch (pad.shape)
    {
    case PadShape::Circle:
        svg.circle(color, cx, cy, pad.parameter[0] / 2.0);
        break;

    case PadShape::Oval:
    {
        // Same construction as the renderer: a wide stroke along the long axis.
        double p0 = pad.parameter[0];
        double p1 = pad.parameter[1];
        if (pad.angle == 0.0)
        {
            if (p0 >= p1) // horizontal
                svg.wideSegment(color, cx - (p0 - p1) / 2.0, cy,
                                cx + (p0 - p1) / 2.0, cy, p1);
            else          // vertical
                svg.wideSegment(color, cx, cy - (p1 - p0) / 2.0,
                                cx, cy + (p1 - p0) / 2.0, p0);
        }
        else
        {
            double half = (p0 - p1) / 2.0;
            double dx = half * qCos(pad.angle);
            double dy = half * qSin(pad.angle);
            svg.wideSegment(color, cx - dx, cy - dy, cx + dx, cy + dy, p1);
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
        svg.polygon(color, corners);
        break;
    }

    case PadShape::Polygon:
        // Approximate as the circumscribing circle, like the parser does
        // for macro polygons.
        svg.circle(color, cx, cy, pad.parameter[0] / 2.0);
        break;

    case PadShape::Macro:
        if (pad.parameterNum >= 2)
        {
            QList<QPointF> corners;
            double hw = pad.parameter[0] / 2.0;
            double hh = pad.parameter[1] / 2.0;
            corners << QPointF(cx - hw, cy - hh) << QPointF(cx + hw, cy - hh)
                    << QPointF(cx + hw, cy + hh) << QPointF(cx - hw, cy + hh);
            svg.polygon(color, corners);
        }
        else if (pad.parameterNum >= 1)
        {
            svg.circle(color, cx, cy, pad.parameter[0] / 2.0);
        }
        break;
    }
}

void writeOutlineTracks(SvgWriter &svg, const Gerber &outline)
{
    // Outline apertures can be very thin; keep the edge visible at 0.15mm min.
    const double minWidth = 0.15 * PRECISIONSCALE;
    for (const Track &t : outline.tracksList)
        svg.line(kOutlineColor, t.pointstart.x(), t.pointstart.y(),
                 t.pointend.x(), t.pointend.y(),
                 qMax(static_cast<double>(t.width), minWidth));
}

} // namespace

bool SvgExport::writeCopper(const Gerber &g, const QString &color,
                            const QString &filePath, QString &errorMsg,
                            bool flipX, const Gerber *outline)
{
    if (g.tracksList.isEmpty() && g.padsList.isEmpty())
    {
        errorMsg = "No copper geometry to export.";
        return false;
    }

    SvgWriter svg(flipX);

    for (const Track &t : g.tracksList)
        svg.wideSegment(color, t.pointstart.x(), t.pointstart.y(),
                        t.pointend.x(), t.pointend.y(), t.width);

    for (const Pad &p : g.padsList)
        writePad(svg, color, p);

    if (outline && !outline->tracksList.isEmpty())
        writeOutlineTracks(svg, *outline);

    return svg.save(filePath, errorMsg);
}

bool SvgExport::writeDrillsOutline(const Gerber *outline,
                                   const ExcellonParser *excellon,
                                   const Preprocess *preprocess,
                                   const QString &filePath, QString &errorMsg,
                                   bool flipX)
{
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

    SvgWriter svg(flipX);

    if (hasOutline)
        writeOutlineTracks(svg, *outline);

    for (auto it = holes.cbegin(); it != holes.cend(); ++it)
    {
        double r = it.key() / 2.0;
        for (const QPoint &pt : it.value())
            svg.circle(kDrillColor, pt.x(), pt.y(), r);
    }

    return svg.save(filePath, errorMsg);
}
