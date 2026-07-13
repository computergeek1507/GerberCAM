#pragma once

#include <QString>
#include "gerber.h"
#include "preprocess.h"
#include "excellonparser.h"

class DxfExport
{
public:
    // Write copper artwork (tracks + pads) from a Gerber layer to a DXF file.
    // layerName is the DXF layer the entities are placed on (e.g. "TOP_COPPER").
    // When outline is non-null its tracks are added on an "OUTLINE" layer.
    // Returns true on success, sets errorMsg on failure.
    static bool writeCopper(const Gerber &g, const QString &layerName,
                            const QString &filePath, QString &errorMsg,
                            bool flipX = false, const Gerber *outline = nullptr);

    // Write the board outline (lines on layer "OUTLINE") to a DXF file.
    // Returns true on success, sets errorMsg on failure.
    static bool writeOutline(const Gerber &outline,
                             const QString &filePath, QString &errorMsg,
                             bool flipX = false);

    // Write drill holes (circles on layer "DRILLS") to a DXF file. Either
    // source may be null; drills come from the Excellon file when loaded,
    // otherwise from the Gerber pad holes in preprocess.
    // Returns true on success, sets errorMsg on failure.
    static bool writeDrills(const ExcellonParser *excellon,
                            const Preprocess *preprocess,
                            const QString &filePath, QString &errorMsg,
                            bool flipX = false);
};
