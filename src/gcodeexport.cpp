#include "gcodeexport.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMap>
#include <cmath>
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
    bool useInch = (tool.unitType == UnitType::Inch);

    // Internal units are mm-based: 1 mm = PRECISIONSCALE (1e6) units.
    // toUnit converts internal units → output unit (mm or inch).
    double toUnit = useInch ? (1.0 / (PRECISIONSCALE * 25.4))
                            : (1.0 / PRECISIONSCALE);

    // All defaults are in mm; divide by 25.4 when inch output is selected.
    double safeZ    = useInch ? kSafeZmm / 25.4 : kSafeZmm;

    // Total engraving depth from the Toolpath Parameter UI depth field.
    double totalDepth_mm = (parm.depth > 0.0) ? parm.depth : kDefaultDepthmm;
    double totalDepth    = useInch ? totalDepth_mm / 25.4 : totalDepth_mm;

    // Step depth per pass: use maxStepDepth when it's a positive fraction of total depth.
    // If unset or >= total depth, a single pass cuts to full depth.
    double step_mm   = (tool.maxStepDepth > 0.0 && tool.maxStepDepth < totalDepth_mm)
                           ? tool.maxStepDepth : totalDepth_mm;
    double step      = useInch ? step_mm / 25.4 : step_mm;
    int    numPasses = static_cast<int>(std::ceil(totalDepth_mm / step_mm));

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
    out << "(Tool: " << tool.name << ")\n";
    out << "(Tool diameter: "
        << QString::number(tool.diameter, 'f', useInch ? 4 : 2)
        << (useInch ? " in" : " mm") << ")\n";
    out << "(Feedrate: " << QString::number(feedrate, 'f', 1)
        << (useInch ? " in/min" : " mm/min") << ")\n";
    out << "(Total depth: -" << QString::number(totalDepth, 'f', zprec)
        << (useInch ? " in" : " mm") << ")\n";
    if (numPasses > 1)
        out << "(Step depth:  -" << QString::number(step, 'f', zprec)
            << (useInch ? " in" : " mm") << ", " << numPasses << " passes)\n";
    out << "(Isolation rings: " << parm.isolationRings << ")\n";
    out << "(Paths:    " << tp.totalToolpath.size() << ")\n";
    if (!tp.clearingPaths.empty())
        out << "(Clearing segments: " << tp.clearingPaths.size() << ")\n";
    if (flipX) out << "(Mirror:   X axis flipped)\n";
    out << "\n";

    out << "G90\n";                            // absolute positioning
    out << (useInch ? "G20\n" : "G21\n");     // unit selection
    out << "G17\n";                            // XY plane
    out << "M3 S" << static_cast<int>(spindle) << "\n"; // spindle on CW
    out << "G1 F" << QString::number(feedrate, 'f', 1) << "\n";
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

        double x0 = path.at(0).X * toUnit * xSign;
        double y0 = path.at(0).Y * toUnit;

        // Rapid to path start at safe height
        out << "G0 X" << QString::number(x0, 'f', prec)
            << " Y" << QString::number(y0, 'f', prec) << "\n";

        // Multi-pass: each pass cuts one step deeper until total depth is reached
        for (int pass = 1; pass <= numPasses; ++pass)
        {
            double passDepth = std::min(step * pass, totalDepth);

            if (pass > 1)
            {
                // Retract and reposition at path start for next pass
                out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
                out << "G0 X" << QString::number(x0, 'f', prec)
                    << " Y" << QString::number(y0, 'f', prec) << "\n";
            }

            // Plunge to this pass depth
            out << "G1 Z-" << QString::number(passDepth, 'f', zprec)
                << " F" << QString::number(plunge, 'f', 1) << "\n";
            //out << "G1 F" << QString::number(feedrate, 'f', 1) << "\n";

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
        }

        // Retract after all passes
        out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
        out << "\n";
    }

    // -------------------------------------------------------
    // Footer
    // -------------------------------------------------------
    // -------------------------------------------------------
    // Copper clearing raster (open segments, no polygon close)
    // -------------------------------------------------------
    if (!tp.clearingPaths.empty())
    {
        out << "(=== Copper Clearing ===)\n";
        int clearIdx = 0;
        for (const auto &seg : tp.clearingPaths)
        {
            if (seg.size() < 2) continue;

            ++clearIdx;
            out << "(--- Clear " << clearIdx << " ---)\n";

            double x0 = seg.front().X * toUnit * xSign;
            double y0 = seg.front().Y * toUnit;

            out << "G0 X" << QString::number(x0, 'f', prec)
                << " Y" << QString::number(y0, 'f', prec) << "\n";

            for (int pass = 1; pass <= numPasses; ++pass)
            {
                double passDepth = std::min(step * pass, totalDepth);
                if (pass > 1)
                {
                    out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
                    out << "G0 X" << QString::number(x0, 'f', prec)
                        << " Y" << QString::number(y0, 'f', prec) << "\n";
                }
                out << "G1 Z-" << QString::number(passDepth, 'f', zprec)
                    << " F" << QString::number(plunge, 'f', 1) << "\n";
                out << "G1 F" << QString::number(feedrate, 'f', 1) << "\n";
                for (size_t i = 1; i < seg.size(); ++i)
                {
                    double x = seg.at(i).X * toUnit * xSign;
                    double y = seg.at(i).Y * toUnit;
                    out << "X" << QString::number(x, 'f', prec)
                        << " Y" << QString::number(y, 'f', prec) << "\n";
                }
                // No polygon close — open segment
            }
            out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
            out << "\n";
        }
    }

    out << "M5\n";   // spindle off
    out << "M30\n";  // end of program

    file.close();
    return true;
}

bool GcodeExport::writeDrills(const Preprocess& pp, const Setting& s,
    const QString& filePath, QString& errorMsg,
    bool flipX)
{
    QMap<qint64, QList<QPoint>> holes;
    for (const Net& net : pp.netList) {
        for (const Element& e : net.elements) {
            if (e.elementType == ElementType::Pad && e.pad.hole > 0) {
                holes[e.pad.hole].append(e.pad.point);
            }
        }
    }

    if (holes.isEmpty())
    {
        errorMsg = "No holes found. Run Hole Identify first.";
        return false;
    }

    return writeDrillsImpl(holes, s, filePath, errorMsg, flipX, "Gerber");
}

bool GcodeExport::writeDrills(const ExcellonParser &exc, const Setting &s,
                              const QString &filePath, QString &errorMsg,
                              bool flipX)
{
    QMap<qint64, QList<QPoint>> holes = exc.holesByDiameter();

    if (holes.isEmpty())
    {
        errorMsg = "No holes in the Excellon data.";
        return false;
    }

    return writeDrillsImpl(holes, s, filePath, errorMsg, flipX, "Excellon");
}

bool GcodeExport::writeDrillsBore(const Preprocess &pp, const Setting &s,
                                  const QString &filePath, QString &errorMsg,
                                  bool flipX)
{
    QMap<qint64, QList<QPoint>> holes;
    for (const Net &net : pp.netList)
        for (const Element &e : net.elements)
            if (e.elementType == ElementType::Pad && e.pad.hole > 0)
                holes[e.pad.hole].append(e.pad.point);

    if (holes.isEmpty())
    {
        errorMsg = "No holes found. Run Hole Identify first.";
        return false;
    }

    return writeDrillsImpl(holes, s, filePath, errorMsg, flipX, "Gerber", true);
}

bool GcodeExport::writeDrillsBore(const ExcellonParser &exc, const Setting &s,
                                  const QString &filePath, QString &errorMsg,
                                  bool flipX)
{
    QMap<qint64, QList<QPoint>> holes = exc.holesByDiameter();

    if (holes.isEmpty())
    {
        errorMsg = "No holes in the Excellon data.";
        return false;
    }

    return writeDrillsImpl(holes, s, filePath, errorMsg, flipX, "Excellon", true);
}

bool GcodeExport::writeDrillsImpl(const QMap<qint64, QList<QPoint>> &holesByDiameter,
                                  const Setting &s, const QString &filePath,
                                  QString &errorMsg, bool flipX,
                                  const QString &sourceLabel,
                                  bool circularBore)
{
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
    bool useInch = (dTool.unitType == UnitType::Inch);

    double toUnit = useInch ? (1.0 / (PRECISIONSCALE * 25.4))
                            : (1.0 / PRECISIONSCALE);
    double safeZ  = useInch ? kSafeZmm / 25.4 : kSafeZmm;

    // Total drill depth from the Toolpath Parameter UI depth field.
    double totalDepth_mm = (parm.depth > 0.0) ? parm.depth : kDefaultDrillDepthmm;
    double totalDepth    = useInch ? totalDepth_mm / 25.4 : totalDepth_mm;

    // Peck step: use maxStepDepth when set and less than total depth.
    // If unset or >= total depth, treat as a single full-depth plunge.
    double step_mm  = (dTool.maxStepDepth > 0.0 && dTool.maxStepDepth < totalDepth_mm)
                          ? dTool.maxStepDepth : totalDepth_mm;
    double step     = useInch ? step_mm / 25.4 : step_mm;
    int    numPecks = static_cast<int>(std::ceil(totalDepth_mm / step_mm));

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
    // Circular bore parameters (computed even when not used)
    double toolDiamMm = dTool.width;
    double stepoverMm = (toolDiamMm > 0.0) ? toolDiamMm * 0.5 : 0.0;
    double stepover   = useInch ? stepoverMm / 25.4 : stepoverMm;
    double toolDiam   = useInch ? toolDiamMm / 25.4 : toolDiamMm;
    // When flipping X, mirror arc direction (G2↔G3) to maintain CW cut in world coords.
    const char *arcDir = flipX ? "G3" : "G2";

    out << "(GerberCAM - Drill Export [" << sourceLabel << "])\n";
    out << "(Generated: "
        << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << ")\n";
    if (circularBore)
    {
        out << "(Mode:       Circular Bore)\n";
        out << "(End mill:   " << dTool.name << ")\n";
        out << "(Tool width: " << QString::number(toolDiam, 'f', useInch ? 4 : 2)
            << (useInch ? " in" : " mm") << ")\n";
        out << "(Step-over:  " << QString::number(stepover, 'f', useInch ? 4 : 2)
            << (useInch ? " in" : " mm") << " [50% tool diam])\n";
    }
    out << "(Holes: " << totalHoles
        << ", Diameters: " << holesByDiameter.size() << ")\n";
    out << "(Drill depth: -" << QString::number(totalDepth, 'f', zprec)
        << (useInch ? " in" : " mm") << ")\n";
    if (numPecks > 1)
        out << "(Peck depth:  -" << QString::number(step, 'f', zprec)
            << (useInch ? " in" : " mm") << ", " << numPecks << " pecks per hole)\n";
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

        // Determine if this hole diameter needs circular boring.
        // In bore mode: bore only if the hole is larger than the tool.
        double holeDiamMm = holeDiam / static_cast<double>(PRECISIONSCALE);
        bool doBore = circularBore && (toolDiamMm > 0.0) && (holeDiamMm > toolDiamMm);

        for (const QPoint &pt : holes)
        {
            double cx = pt.x() * toUnit * xSign;
            double cy = pt.y() * toUnit;
            out << "G0 X" << QString::number(cx, 'f', prec)
                << " Y" << QString::number(cy, 'f', prec) << "\n";

            if (!doBore)
            {
                // Straight peck drilling
                for (int peck = 1; peck <= numPecks; ++peck)
                {
                    double peckDepth = std::min(step * peck, totalDepth);
                    out << "G1 Z-" << QString::number(peckDepth, 'f', zprec)
                        << " F" << QString::number(toolFeed, 'f', 1) << "\n";
                    if (peck < numPecks)
                        out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
                }
            }
            else
            {
                // Circular bore: concentric arcs expanding from centre to hole wall.
                // maxR = distance from hole centre to tool-centre at full bore width.
                double maxR_mm = (holeDiamMm - toolDiamMm) / 2.0;
                double maxR    = useInch ? maxR_mm / 25.4 : maxR_mm;

                for (int peck = 1; peck <= numPecks; ++peck)
                {
                    double peckDepth = std::min(step * peck, totalDepth);

                    // Plunge at centre
                    out << "G1 Z-" << QString::number(peckDepth, 'f', zprec)
                        << " F" << QString::number(toolFeed, 'f', 1) << "\n";
                    out << "G1 F" << QString::number(toolFeed, 'f', 1) << "\n";

                    // Spiral outward: one full circle per step-over increment
                    double r = stepover;
                    while (r < maxR - 1e-6)
                    {
                        out << "G1 X" << QString::number(cx + r, 'f', prec)
                            << " Y" << QString::number(cy,      'f', prec) << "\n";
                        out << arcDir
                            << " X" << QString::number(cx + r, 'f', prec)
                            << " Y" << QString::number(cy,      'f', prec)
                            << " I" << QString::number(-r,      'f', prec)
                            << " J0\n";
                        r += stepover;
                    }

                    // Final finishing pass at exact wall radius
                    out << "G1 X" << QString::number(cx + maxR, 'f', prec)
                        << " Y" << QString::number(cy,          'f', prec) << "\n";
                    out << arcDir
                        << " X" << QString::number(cx + maxR, 'f', prec)
                        << " Y" << QString::number(cy,         'f', prec)
                        << " I" << QString::number(-maxR,      'f', prec)
                        << " J0\n";

                    // Return to centre before next peck or retract
                    out << "G1 X" << QString::number(cx, 'f', prec)
                        << " Y" << QString::number(cy,   'f', prec) << "\n";

                    if (peck < numPecks)
                        out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
                }
            }

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
    bool useInch = (tool.unitType == UnitType::Inch);

    double toUnit = useInch ? (1.0 / (PRECISIONSCALE * 25.4))
                            : (1.0 / PRECISIONSCALE);
    double safeZ = useInch ? kSafeZmm / 25.4 : kSafeZmm;

    // Total milling depth from the Toolpath Parameter UI depth field.
    double totalDepth_mm = (parm.depth > 0.0) ? parm.depth : kDefaultDepthmm;
    double totalDepth    = useInch ? totalDepth_mm / 25.4 : totalDepth_mm;

    // Step depth per pass: use maxStepDepth when set and less than total depth.
    double step_mm   = (tool.maxStepDepth > 0.0 && tool.maxStepDepth < totalDepth_mm)
                           ? tool.maxStepDepth : totalDepth_mm;
    double step      = useInch ? step_mm / 25.4 : step_mm;
    int    numPasses = static_cast<int>(std::ceil(totalDepth_mm / step_mm));

    double feed_mm   = (tool.feedrate > 0.0)       ? tool.feedrate       : kDefaultFeedratemm;
    double feedrate  = useInch ? feed_mm / 25.4 : feed_mm;
    double plunge_mm = (tool.maxPlungeSpeed > 0.0) ? tool.maxPlungeSpeed : kDefaultPlungemm;
    double plunge    = useInch ? plunge_mm / 25.4 : plunge_mm;
    double spindle   = (tool.spindleSpeed > 0.0)   ? tool.spindleSpeed   : kDefaultSpindle;

    int prec  = useInch ? 6 : 4;
    int zprec = useInch ? 4 : 3;
    double xSign = flipX ? -1.0 : 1.0;

    out << "(GerberCAM - Outline Cut)\n";
    out << "(Generated: "
        << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        << ")\n";
    out << "(Tool: " << tool.name << ")\n";
    out << "(Feedrate:    " << QString::number(feedrate, 'f', 1)
        << (useInch ? " in/min" : " mm/min") << ")\n";
    out << "(Total depth: -" << QString::number(totalDepth, 'f', zprec)
        << (useInch ? " in" : " mm") << ")\n";
    if (numPasses > 1)
        out << "(Step depth:  -" << QString::number(step, 'f', zprec)
            << (useInch ? " in" : " mm") << ", " << numPasses << " passes)\n";
    out << "(Segments: " << outline.tracksList.size() << ")\n";
    if (flipX) out << "(Mirror:   X axis flipped)\n";
    out << "\n";

    out << "G90\n";
    out << (useInch ? "G20\n" : "G21\n");
    out << "G17\n";
    out << "M3 S" << static_cast<int>(spindle) << "\n";
    out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
    out << "\n";

    // Multi-pass: one full outline traversal per pass, each going one step deeper.
    // cutting/lastX/lastY reset each pass so closed outlines always re-plunge correctly.
    for (int pass = 1; pass <= numPasses; ++pass)
    {
        double passDepth = std::min(step * pass, totalDepth);

        if (numPasses > 1)
            out << "(--- Pass " << pass << ": Z-"
                << QString::number(passDepth, 'f', zprec) << " ---)\n";

        bool cutting = false;
        double lastX = 0, lastY = 0;

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
                if (cutting)
                    out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
                out << "G0 X" << QString::number(sx, 'f', prec)
                    << " Y" << QString::number(sy, 'f', prec) << "\n";
                out << "G1 Z-" << QString::number(passDepth, 'f', zprec)
                    << " F" << QString::number(plunge, 'f', 1) << "\n";
                out << "G1 F" << QString::number(feedrate, 'f', 1) << "\n";
                cutting = true;
            }

            out << "G1 X" << QString::number(ex, 'f', prec)
                << " Y" << QString::number(ey, 'f', prec) << "\n";
            lastX = ex;
            lastY = ey;
        }

        if (cutting)
            out << "G0 Z" << QString::number(safeZ, 'f', zprec) << "\n";
        out << "\n";
    }

    out << "M5\n";
    out << "M30\n";

    file.close();
    return true;
}
