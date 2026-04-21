#include "gcodeexport.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMap>
#include "scale.h"

bool GcodeExport::write(const Toolpath &tp, const Setting &s,
                        const QString &filePath, QString &errorMsg,
                        bool flipX)
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
    const Tool &tool = s.getEngravingTool().value_or(Tool{});
    const CuttingParm& parm = s.engravingParm;
    bool useInch = (tool.unitType == "Inch");

    // Internal units are mm-based: 1 mm = PRECISIONSCALE (1e6) units.
    // toUnit converts internal units → output unit (mm or inch).
    double toUnit = useInch ? (1.0 / (PRECISIONSCALE * 25.4))
                            : (1.0 / PRECISIONSCALE);

    // All defaults are in mm; divide by 25.4 when inch output is selected.
    double safeZ    = useInch ? kSafeZmm / 25.4 : kSafeZmm;

    double depth_mm = (tool.maxStepDepth > 0.0) ? tool.maxStepDepth : parm.depth;
    double depth    = useInch ? depth_mm / 25.4 : depth_mm;

    double feed_mm  = (tool.feedrate > 0.0)       ? tool.feedrate       : kDefaultFeedratemm;
    double feedrate = useInch ? feed_mm / 25.4 : feed_mm;

    double plunge_mm = (tool.maxPlungeSpeed > 0.0) ? tool.maxPlungeSpeed : kDefaultPlungemm;
    double plunge    = useInch ? plunge_mm / 25.4 : plunge_mm;

    double spindle  = (tool.spindleSpeed > 0.0)
                          ? tool.spindleSpeed : kDefaultSpindle;

    int prec = useInch ? 6 : 4; // decimal places for XY coords
    int zprec = useInch ? 4 : 3;
    double xSign = flipX ? -1.0 : 1.0;

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
    if(flipX) out << "(Mirror:   X axis flipped)\n";
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
        double x0 = path.at(0).X * toUnit * xSign;
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
            double x = path.at(i).X * toUnit * xSign;
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
                              const QString &filePath, QString &errorMsg,
                              bool flipX)
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
    const Tool &dTool = s.getDrillTool().value_or(Tool{});
    const CuttingParm& parm = s.drillParm;
    bool useInch = (dTool.unitType == "Inch");

    double toUnit = useInch ? (1.0 / (PRECISIONSCALE * 25.4))
                            : (1.0 / PRECISIONSCALE);
    double safeZ  = useInch ? kSafeZmm / 25.4 : kSafeZmm;

    double depth_mm = (parm.depth > dTool.maxStepDepth) ? dTool.maxStepDepth : parm.depth;
    double depth    = useInch ? depth_mm / 25.4 : depth_mm;

    double plunge_mm = (dTool.maxPlungeSpeed > 0.0) ? dTool.maxPlungeSpeed : kDefaultDrillFeedmm;
    double plunge    = useInch ? plunge_mm / 25.4 : plunge_mm;

    double spindle = (dTool.spindleSpeed > 0.0) ? dTool.spindleSpeed : kDefaultDrillRPM;

    int prec  = useInch ? 6 : 4;
    int zprec = useInch ? 4 : 3;
    double xSign = flipX ? -1.0 : 1.0;

    // Count total holes
    int totalHoles = 0;
    for (const auto &list : holesByDiameter) totalHoles += list.size();

	auto drills = s.getDrillList();

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
    if(flipX) out << "(Mirror:     X axis flipped)\n";
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

        double diamMM  = holeDiam / PRECISIONSCALE; // always in mm
        double diamOut = useInch ? diamMM / 25.4 : diamMM;

        // Try to find a matching drill bit in the library (closest diameter, both in mm)
        QString toolName = "";
        double  toolFeed = plunge;
        double  toolRPM  = spindle;
        if (!drills.empty())
        {
            double bestDiff = 1e9;
            for (const Tool &t : drills)
            {
                double diff = qAbs(t.width - diamMM);
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
            double x = pt.x() * toUnit * xSign;
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


bool GcodeExport::writeOutline(const Gerber &outline, const Setting &s,
                               const QString &filePath, QString &errorMsg,
                               bool flipX)
{
    if (outline.tracksList.isEmpty())
    {
        errorMsg = "No outline tracks found in the edge cut file.";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        errorMsg = "Cannot open file for writing: " + file.errorString();
        return false;
    }

    QTextStream out(&file);

    const Tool &tool = s.getCutTool().value_or(Tool{});
    const CuttingParm& parm = s.cutParm;
    bool useInch = (tool.unitType == "Inch");

    double toUnit = useInch ? (1.0 / (PRECISIONSCALE * 25.4))
                            : (1.0 / PRECISIONSCALE);
    double safeZ    = useInch ? kSafeZmm / 25.4 : kSafeZmm;

    double depth_mm = parm.depth;
    double step_depth_mm = (parm.depth > tool.maxStepDepth) ? tool.maxStepDepth : parm.depth;

    int passes = std::ceil(depth_mm / step_depth_mm);

    double depth    = useInch ? depth_mm / 25.4 : depth_mm;
    double step_depth = useInch ? step_depth_mm / 25.4 : step_depth_mm;

    double feed_mm  = (tool.feedrate > 0.0) ? tool.feedrate : kDefaultFeedratemm;
    double feedrate = useInch ? feed_mm / 25.4 : feed_mm;
    double plunge_mm = (tool.maxPlungeSpeed > 0.0) ? tool.maxPlungeSpeed : kDefaultPlungemm;
    double plunge    = useInch ? plunge_mm / 25.4 : plunge_mm;
    double spindle  = (tool.spindleSpeed > 0.0) ? tool.spindleSpeed : kDefaultSpindle;

    int prec  = useInch ? 6 : 4;
    int zprec = useInch ? 4 : 3;
    double xSign = flipX ? -1.0 : 1.0;

    out << "(GerberCAM - Outline Cut)\n";
    out << "(Generated: "
        << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        << ")\n";
    out << "(Feedrate: " << feedrate << (useInch ? " in/min" : " mm/min") << ")\n";
    out << "(Depth:    -" << QString::number(depth, 'f', zprec)
        << (useInch ? " in" : " mm") << ")\n";
    out << "(Segments: " << outline.tracksList.size() << ")\n";
    if(flipX) out << "(Mirror:   X axis flipped)\n";
    out << "\n";

    out << "G90\n";
    out << (useInch ? "G20\n" : "G21\n");
    out << "G17\n";
    out << "M3 S" << static_cast<int>(spindle) << "\n";
    out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
    out << "\n";

    bool cutting = false;
    double lastX = 0, lastY = 0;

    for (int pass = 1; pass <= passes; ++pass) {

        for (int i = 0; i < outline.tracksList.size(); ++i)
        {
            const Track& t = outline.tracksList.at(i);
            double sx = t.pointstart.x() * toUnit * xSign;
            double sy = t.pointstart.y() * toUnit;
            double ex = t.pointend.x() * toUnit * xSign;
            double ey = t.pointend.y() * toUnit;

            bool connected = cutting
                && qAbs(sx - lastX) < 0.001
                && qAbs(sy - lastY) < 0.001;

            if (!connected)
            {
                double tdepth = pass * step_depth;

                tdepth = std::min(tdepth, depth);

                if (cutting)
                    out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
                out << "G0 X" << QString::number(sx, 'f', prec)
                    << " Y" << QString::number(sy, 'f', prec) << "\n";
                out << "G1 Z-" << QString::number(tdepth, 'f', zprec)
                    << " F" << QString::number(plunge, 'f', 1) << "\n";
                out << "G1 F" << QString::number(feedrate, 'f', 1) << "\n";
                cutting = true;
            }

            out << "G1 X" << QString::number(ex, 'f', prec)
                << " Y" << QString::number(ey, 'f', prec) << "\n";
            lastX = ex;
            lastY = ey;
        }
    }

    if (cutting)
        out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";

    out << "\n";
    out << "M5\n";
    out << "M30\n";

    file.close();
    return true;
}
