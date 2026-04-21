#pragma once

#include <QString>
#include "toolpath.h"
#include "setting.h"

class GcodeExport
{
public:
    // Write isolation-routing G-code from tp to filePath.
    // Returns true on success, sets errorMsg on failure.
    static bool write(const Toolpath &tp, const Setting &s,
                      const QString &filePath, QString &errorMsg);

private:
    // Safe retract height above the PCB surface (in working units).
    static constexpr double kSafeZInch = 0.100;

    // Default cutting depth when maxStepDepth is not configured.
    static constexpr double kDefaultDepthInch = 0.005;

    // Default feedrates (in/min) when not configured.
    static constexpr double kDefaultFeedrate   = 30.0;
    static constexpr double kDefaultPlunge     = 10.0;
    static constexpr double kDefaultSpindle    = 10000.0;
};
