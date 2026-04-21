#include "gcodeexport.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMap>
#include "scale.h"

bool GcodeExport::write(const Toolpath &tp, const Setting &s,
                        const QString &filePath, QString &errorMsg)
{
    if (tp.totalToolpath.empty())
    {
        errorMsg = "No toolpath to export. Generate the toolpath first.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file for writing: " + file.errorString();
        return false;
    }

    QTextStream out(&file);

    // -------------------------------------------------------
    // Resolve parameters from settings, falling back to safe defaults
    // -------------------------------------------------------
    const Tool &tool = s.engravingTool;

    bool useInch = (tool.unitType != "MM");

    // Unit scale: internal units → output unit
    // Internal: 1 inch = PRECISIONSCALE (1e6) units
    double toUnit = useInch ? (1.0 / PRECISIONSCALE)
                            : (25.4 / PRECISIONSCALE); // → mm

    double safeZ  = useInch ? kSafeZInch : kSafeZInch * 25.4;

    double depth  = (tool.maxStepDepth > 0.0)
                        ? tool.maxStepDepth
                        : kDefaultDepthInch;
    if (!useInch) depth *= 25.4;

    double feedrate = (tool.feedrate > 0.0)
                          ? tool.feedrate : kDefaultFeedrate;
    double plunge   = (tool.maxPlungeSpeed > 0.0)
                          ? tool.maxPlungeSpeed : kDefaultPlunge;
    double spindle  = (tool.spindleSpeed > 0.0)
                          ? tool.spindleSpeed : kDefaultSpindle;

    int prec = useInch ? 6 : 4; // decimal places for XY coords
    int zprec = useInch ? 4 : 3;

    // -------------------------------------------------------
    // Header
    // -------------------------------------------------------
    out << "(GerberCAM - Isolation Routing)\n";
    out << "(Generated: "
        << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        << ")\n";
    out << "(Tool diameter: "
        << QString::number(tool.diameter, 'f', useInch ? 4 : 2)
        << (useInch ? " in" : " mm") << ")\n";
    out << "(Feedrate: " << feedrate << (useInch ? " in/min" : " mm/min") << ")\n";
    out << "(Depth:    -" << QString::number(depth,'f',zprec)
        << (useInch ? " in" : " mm") << ")\n";
    out << "(Paths:    " << tp.totalToolpath.size() << ")\n";
    out << "\n";

    out << "G90\n";                            // absolute positioning
    out << (useInch ? "G20\n" : "G21\n");     // unit selection
    out << "G17\n";                            // XY plane
    out << "M3 S" << static_cast<int>(spindle) << "\n"; // spindle on CW
    out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n"; // safe height
    out << "\n";

    // -------------------------------------------------------
    // Paths
    // -------------------------------------------------------
    int pathIdx = 0;
    for (const auto &path : tp.totalToolpath)
    {
        if (path.empty())
            continue;

        ++pathIdx;
        out << "(--- Path " << pathIdx << " ---)\n";

        // First point: rapid XY move at safe Z
        double x0 = path.at(0).X * toUnit;
        double y0 = path.at(0).Y * toUnit;
        out << "G0 X" << QString::number(x0, 'f', prec)
            << " Y" << QString::number(y0, 'f', prec) << "\n";

        // Plunge
        out << "G1 Z-" << QString::number(depth, 'f', zprec)
            << " F" << QString::number(plunge, 'f', 1) << "\n";

        // Cut through remaining points
        out << "G1 F" << QString::number(feedrate, 'f', 1) << "\n";
        for (size_t i = 1; i < path.size(); ++i)
        {
            double x = path.at(i).X * toUnit;
            double y = path.at(i).Y * toUnit;
            out << "X" << QString::number(x, 'f', prec)
                << " Y" << QString::number(y, 'f', prec) << "\n";
        }

        // Close the polygon (return to first point)
        out << "X" << QString::number(x0, 'f', prec)
            << " Y" << QString::number(y0, 'f', prec) << "\n";

        // Retract
        out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
        out << "\n";
    }

    // -------------------------------------------------------
    // Footer
    // -------------------------------------------------------
    out << "M5\n";   // spindle off
    out << "M30\n";  // end of program

    file.close();
    return true;
}

bool GcodeExport::writeDrills(const Preprocess &pp, const Setting &s,
                              const QString &filePath, QString &errorMsg)
{
    // -------------------------------------------------------
    // Collect all holes grouped by diameter
    // Key: hole diameter in internal units; Value: list of pad centers
    // -------------------------------------------------------
    QMap<qint64, QList<QPoint>> holesByDiameter;

    for (const Net &net : pp.netList)
    {
        for (const Element &e : net.elements)
        {
            if (e.elementType == 'P' && e.pad.hole > 0)
                holesByDiameter[e.pad.hole].append(e.pad.point);
        }
    }

    if (holesByDiameter.isEmpty())
    {
        errorMsg = "No holes found. Run Hole Identify first.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file for writing: " + file.errorString();
        return false;
    }

    QTextStream out(&file);

    // -------------------------------------------------------
    // Resolve parameters — use drillTool if configured, else defaults
    // -------------------------------------------------------
    const Tool &dTool = s.drillTool;
    bool useInch = (dTool.unitType != "MM");

    double toUnit  = useInch ? (1.0 / PRECISIONSCALE) : (25.4 / PRECISIONSCALE);
    double safeZ   = useInch ? kSafeZInch : kSafeZInch * 25.4;
    double depth   = (dTool.maxStepDepth > 0.0) ? dTool.maxStepDepth
                                                 : kDefaultDrillDepthInch;
    if (!useInch) depth *= 25.4;

    double plunge  = (dTool.maxPlungeSpeed > 0.0) ? dTool.maxPlungeSpeed
                                                   : kDefaultDrillFeed;
    double spindle = (dTool.spindleSpeed > 0.0)   ? dTool.spindleSpeed
                                                   : kDefaultDrillRPM;

    int prec  = useInch ? 6 : 4;
    int zprec = useInch ? 4 : 3;

    // Count total holes
    int totalHoles = 0;
    for (const auto &list : holesByDiameter) totalHoles += list.size();

    // -------------------------------------------------------
    // Header
    // -------------------------------------------------------
    out << "(GerberCAM - Drill Export)\n";
    out << "(Generated: "
        << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << ")\n";
    out << "(Holes: " << totalHoles
        << ", Diameters: " << holesByDiameter.size() << ")\n";
    out << "(Drill depth: -" << QString::number(depth, 'f', zprec)
        << (useInch ? " in" : " mm") << ")\n";
    out << "\n";

    out << "G90\n";
    out << (useInch ? "G20\n" : "G21\n");
    out << "G17\n";
    out << "\n";

    // -------------------------------------------------------
    // One section per unique hole diameter
    // -------------------------------------------------------
    int toolNum = 0;
    for (auto it = holesByDiameter.cbegin(); it != holesByDiameter.cend(); ++it)
    {
        ++toolNum;
        qint64 holeDiam = it.key();
        const QList<QPoint> &holes = it.value();

        double diamInch = holeDiam / PRECISIONSCALE;
        double diamOut  = useInch ? diamInch : diamInch * 25.4;

        // Try to find a matching drill bit in the library (closest diameter)
        QString toolName = "";
        double  toolFeed = plunge;
        double  toolRPM  = spindle;
        if (!s.drillList.isEmpty())
        {
            double bestDiff = 1e9;
            for (const Tool &t : s.drillList)
            {
                double diff = qAbs(t.width - diamInch);
                if (diff < bestDiff)
                {
                    bestDiff = diff;
                    toolName = t.name;
                    if (t.maxPlungeSpeed > 0.0) toolFeed = t.maxPlungeSpeed;
                    if (t.spindleSpeed   > 0.0) toolRPM  = t.spindleSpeed;
                }
            }
        }

        out << "(--- T" << toolNum << ": diameter "
            << QString::number(diamOut, 'f', useInch ? 4 : 2)
            << (useInch ? " in" : " mm");
        if (!toolName.isEmpty()) out << " [" << toolName << "]";
        out << ", " << holes.size() << " hole(s) ---)\n";

        out << "M3 S" << static_cast<int>(toolRPM) << "\n";
        out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";

        for (const QPoint &pt : holes)
        {
            double x = pt.x() * toUnit;
            double y = pt.y() * toUnit;
            out << "G0 X" << QString::number(x, 'f', prec)
                << " Y" << QString::number(y, 'f', prec) << "\n";
            out << "G1 Z-" << QString::number(depth, 'f', zprec)
                << " F" << QString::number(toolFeed, 'f', 1) << "\n";
            out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
        }
        out << "\n";
    }

    // -------------------------------------------------------
    // Footer
    // -------------------------------------------------------
    out << "M5\n";
    out << "M30\n";

    file.close();
    return true;
}
