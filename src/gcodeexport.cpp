#include "gcodeexport.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
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
