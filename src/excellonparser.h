#pragma once

#include <QString>
#include <QList>
#include <QPoint>
#include <QMap>

// One tool entry parsed from an Excellon file
struct ExcellonTool {
    int           number    {0};
    double        diameterMm{0.0}; // always in mm
    QList<QPoint> holes;           // hole centres in internal units (mm × 1e6)
};

class ExcellonParser
{
public:
    // Parse an Excellon (.drl / .exc / .xln) drill file.
    // Returns true on success, sets errorMsg on failure.
    bool parse(const QString &filePath, QString &errorMsg);

    // Parsed tools in definition order; tools with no holes are omitted.
    QList<ExcellonTool> tools;

    // Convenience: holes grouped by internal-unit diameter key.
    // Directly compatible with GcodeExport::writeDrills(map, …).
    QMap<qint64, QList<QPoint>> holesByDiameter() const;

private:
    // --- format state set by the file header ---
    enum class Units    { Metric, Inch };
    enum class ZeroMode { TrailingZerosSuppressed,   // TZ: number is left-aligned
                          LeadingZerosSuppressed };  // LZ: number is right-aligned

    Units    m_units      = Units::Metric;
    ZeroMode m_zeroMode   = ZeroMode::TrailingZerosSuppressed;
    int      m_intDigits  = 3;   // integer  digits in fixed-point coords
    int      m_fracDigits = 3;   // fraction digits in fixed-point coords

    // Convert a single coordinate token ("X" or "Y" portion) → internal units.
    // Handles both decimal-point and fixed-format encodings.
    qint64 parseCoord(const QString &token) const;
};
