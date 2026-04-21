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
    // Safe retract height above the PCB surface (in working units).
    static constexpr double kSafeZInch = 0.100;

    // Default cutting depth when maxStepDepth is not configured.
    static constexpr double kDefaultDepthInch = 0.005;

    // Default drill-through depth (PCB thickness + clearance) if not configured.
    static constexpr double kDefaultDrillDepthInch = 0.075; // ~1.9mm

    // Default feedrates (in/min) when not configured.
    static constexpr double kDefaultFeedrate   = 30.0;
    static constexpr double kDefaultPlunge     = 10.0;
    static constexpr double kDefaultSpindle    = 10000.0;
    static constexpr double kDefaultDrillFeed  = 10.0;
    static constexpr double kDefaultDrillRPM   = 5000.0;
};
