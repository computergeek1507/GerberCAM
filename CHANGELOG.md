# Changelog

Notable changes to GerberCAM. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.9.0] - 2026-07-12

### Added
- DXF export (Machine → Export DXF...): writes separate files for top copper
  (`*_top_copper.dxf`), bottom copper (`*_bottom_copper.dxf`), outline
  (`*_outline.dxf`), and drill holes (`*_drills.dxf`). Output is DXF R2000
  (AC1015) in millimetres; tracks and oval pads are `LWPOLYLINE`s carrying the
  trace width (chosen over R12 `POLYLINE`, which many viewers don't render),
  circle pads and drill holes are circles, rectangle pads are closed
  polylines. The copper files include the board outline as an `OUTLINE` layer.
- SVG export (Machine → Export SVG...): same file set as the DXF export,
  rendered at true scale in millimetres. Tracks and oval pads are round-capped
  strokes, pads and drills are filled shapes; top copper red, bottom copper
  blue, outline green.
- Open Gerber Folder (File → Open Gerber Folder...): scans a folder and
  auto-detects the top copper, bottom copper, outline, and drill files by
  extension (`.gtl`/`.gbl`/`.gm1`/`.gko`/`.drl`/...) and by common naming
  conventions (KiCad `F_Cu`/`B_Cu`/`Edge_Cuts`, `top`/`bottom`/`outline`).
  Prefers plated (non-NPTH) drill files; skips mask/paste/silkscreen layers.
- Layer selection indicator in the status bar ("Viewing: Layer N").
- Command line support: load a Gerber folder or `.gcproj` positionally, or use
  `--folder`, `--project`, `--top`, `--bottom`, `--outline`, `--drill`; batch
  export with `--export-gcode`, `--export-dxf`, `--export-svg <base>` plus
  `--flip`; `--quit` exits after processing for scripted use.
- The left side tabs now drive the view: selecting Layer1/Layer2 switches the
  displayed layer, Outline shows the outline scene, and Drills shows the drill
  holes over the outline. Tab order is now Layer1, Layer2, Outline, Drills,
  Message.

### Changed
- Removed the View → Layer1/Layer2 menu entries; layer selection moved to the
  left side tabs.
- Default build is now Qt 6 (6.6.3 in CI and `VS2022.bat`); Qt 5 still works.
  `windeployqt` runs for Qt 6 Windows builds too, so installers include the
  Qt DLLs.

### Fixed
- Tracks, oval pads, and contours were invisible in the layer preview when
  built with Qt 6: its line stroker doesn't render the huge pen widths our
  nanometre-scale scene needs. Wide segments are now drawn as filled stroke
  paths instead.
- `Gerber::borderRect` was computed from uninitialized min/max members,
  producing a garbage rectangle — zoom-to-fit after loading went to a random
  spot.
- Open Gerber Folder and Load Project now unload the previously loaded board
  first — before, files missing from the new folder (e.g. the drill file)
  lingered from the previous one.
- Switching layers (View → Layer1/Layer2) after loading a two-layer board via
  Open Gerber Folder or Load Project showed a blank view: the combined layer
  scenes were only built by the interactive Add layer flow. The view now also
  zooms to fit the board after a folder open.
- Gerber files that explicitly declare inch units (`%MOIN*%`) were not
  converted to millimetres, producing G-code and DXF output 25.4× too small.
  Files without a `%MO` statement (defaulting to inches) were unaffected.

## [0.8.0]

Fork by @computergeek1507, continuing @claus007's fork of the original
project by @malichao.

### Added
- Excellon drill file parser and drill view overlay.
- Outline (edge cuts) Gerber support.
- G-code export for isolation toolpaths, outline cutting, and drill holes,
  including multi-pass depth stepping.
- Drill hole G-code export with circular boring for holes larger than the
  drill/end mill.
- Full copper clearing (milling all empty areas).
- Multiple isolation rings.
- Board flipping (mirror X) for single-sided milling of the bottom layer.
- Project save/load (`.gcproj`).
- Tool library and settings rework with JSON storage (nlohmann json),
  logging (spdlog), and magic_enum.
- Inno Setup installer script, GitHub Actions Windows/Ubuntu builds, and
  Flatpak packaging.

### Changed
- Build system switched to CMake; compiles with VS2022 and Qt 5.15 (Qt 6
  should work too).
- Gerber parser updated to support aperture macros used by KiCad 9.
- Internal units switched to millimetre-based (1 mm = 1e6 units).
