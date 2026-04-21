#pragma once

#include <QString>
#include "toolpath.h"
#include "preprocess.h"
#include "setting.h"

class GcodeExport
{
public:
    // Write isolation-routing G-code from tp to filePath.
    // Returns true on success, sets errorMsg on failure.
    static bool write(const Toolpath &tp, const Setting &s,
                      const QString &filePath, QString &errorMsg,
                      bool flipX = false);

    // Write drill G-code from the holes in pp to filePath.
    // Returns true on success, sets errorMsg on failure.
    static bool writeDrills(const Preprocess &pp, const Setting &s,
                            const QString &filePath, QString &errorMsg,
                            bool flipX = false);

private:
    // All defaults are in mm (or RPM where noted).
    // When inch output is requested, these are divided by 25.4.

    // Safe retract height above the PCB surface.
    static constexpr double kSafeZmm          = 2.54;    // 0.1 inch

    // Default isolation cutting depth (engrave depth into substrate).
    static constexpr double kDefaultDepthmm   = 0.127;   // ~5 mil

    // Default drill-through depth (PCB thickness + clearance).
    static constexpr double kDefaultDrillDepthmm = 1.905; // ~75 mil / 1.9mm

    // Default feedrates in mm/min.
    static constexpr double kDefaultFeedratemm  = 762.0;  // 30 in/min
    static constexpr double kDefaultPlungemm    = 254.0;  // 10 in/min
    static constexpr double kDefaultDrillFeedmm = 254.0;  // 10 in/min

    // Spindle speeds in RPM (unit-independent).
    static constexpr double kDefaultSpindle    = 10000.0;
    static constexpr double kDefaultDrillRPM   = 5000.0;
};
