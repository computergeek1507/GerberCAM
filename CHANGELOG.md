# Changelog

Notable changes to GerberCAM. The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- DXF export (Machine → Export DXF...): writes separate files for top copper
  (`*_top_copper.dxf`), bottom copper (`*_bottom_copper.dxf`), and
  drills/outline (`*_drills_outline.dxf`). Output is DXF R12 in millimetres;
  tracks and oval pads are polylines carrying the trace width, circle pads and
  drill holes are circles, rectangle pads are closed polylines. The board
  outline is included as an `OUTLINE` layer in every file.
- SVG export (Machine → Export SVG...): same three files as the DXF export,
  rendered at true scale in millimetres. Tracks and oval pads are round-capped
  strokes, pads and drills are filled shapes; top copper red, bottom copper
  blue, outline green. The board outline is included in every file.
- Open Gerber Folder (File → Open Gerber Folder...): scans a folder and
  auto-detects the top copper, bottom copper, outline, and drill files by
  extension (`.gtl`/`.gbl`/`.gm1`/`.gko`/`.drl`/...) and by common naming
  conventions (KiCad `F_Cu`/`B_Cu`/`Edge_Cuts`, `top`/`bottom`/`outline`).
  Prefers plated (non-NPTH) drill files; skips mask/paste/silkscreen layers.
- Layer selection indicator: the View menu Layer1/Layer2 entries now show an
  exclusive checkmark, and the status bar shows "Viewing: Layer N".

### Fixed
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
