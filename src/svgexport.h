#pragma once

#include <QString>
#include "gerber.h"
#include "preprocess.h"
#include "excellonparser.h"

class SvgExport
{
public:
    // Write copper artwork (tracks + pads) from a Gerber layer to an SVG file.
    // color is the fill/stroke color for the copper (e.g. "#c83232").
    // When outline is non-null its tracks are added in green.
    // Returns true on success, sets errorMsg on failure.
    static bool writeCopper(const Gerber &g, const QString &color,
                            const QString &filePath, QString &errorMsg,
                            bool flipX = false, const Gerber *outline = nullptr);

    // Write drill holes (filled circles) and the board outline (lines) into a
    // single SVG file. Any source may be null; drills come from the Excellon
    // file when loaded, otherwise from the Gerber pad holes in preprocess.
    // Returns true on success, sets errorMsg on failure.
    static bool writeDrillsOutline(const Gerber *outline,
                                   const ExcellonParser *excellon,
                                   const Preprocess *preprocess,
                                   const QString &filePath, QString &errorMsg,
                                   bool flipX = false);
};
