/*
Copyright (c) <2014> <Lichao Ma>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/GerberCAM.ico"));
    // Set before the parser runs so --version reports correctly.
    QCoreApplication::setApplicationName(PROJECT_NAME);
    QCoreApplication::setApplicationVersion(PROJECT_VER);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "GerberCAM - PCB gerber to CNC toolpath converter.\n"
        "Load a board from the command line and optionally export G-code, "
        "DXF or SVG in batch.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("path",
        "Gerber folder (auto-detects layers) or .gcproj project file.", "[path]");

    QCommandLineOption optFolder({ "f", "folder" }, "Gerber folder to auto-detect and load.", "dir");
    QCommandLineOption optProject({ "p", "project" }, "GerberCAM project file (.gcproj).", "file");
    QCommandLineOption optTop("top", "Top copper Gerber file.", "file");
    QCommandLineOption optBottom("bottom", "Bottom copper Gerber file.", "file");
    QCommandLineOption optOutline("outline", "Outline / edge cuts Gerber file.", "file");
    QCommandLineOption optDrill("drill", "Excellon drill file.", "file");
    QCommandLineOption optGcode("export-gcode", "Export G-code to <base>.nc/_bottom/_drill/_outline.", "base");
    QCommandLineOption optDxf("export-dxf", "Export DXF to <base>_top_copper/_bottom_copper/_outline/_drills.", "base");
    QCommandLineOption optSvg("export-svg", "Export SVG to <base>_top_copper/_bottom_copper/_outline/_drills.", "base");
    QCommandLineOption optFlip("flip", "Flip the board (mirror X) before exporting.");
    QCommandLineOption optQuit({ "q", "quit" }, "Exit after loading and exporting (batch mode).");
    parser.addOption(optFolder);
    parser.addOption(optProject);
    parser.addOption(optTop);
    parser.addOption(optBottom);
    parser.addOption(optOutline);
    parser.addOption(optDrill);
    parser.addOption(optGcode);
    parser.addOption(optDxf);
    parser.addOption(optSvg);
    parser.addOption(optFlip);
    parser.addOption(optQuit);
    parser.process(a);

    CliOptions opts;
    opts.folder    = parser.value(optFolder);
    opts.project   = parser.value(optProject);
    opts.top       = parser.value(optTop);
    opts.bottom    = parser.value(optBottom);
    opts.outline   = parser.value(optOutline);
    opts.drill     = parser.value(optDrill);
    opts.gcodeBase = parser.value(optGcode);
    opts.dxfBase   = parser.value(optDxf);
    opts.svgBase   = parser.value(optSvg);
    opts.flip      = parser.isSet(optFlip);
    opts.quit      = parser.isSet(optQuit);

    // A bare positional argument is a folder or a project file.
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty())
    {
        QFileInfo fi(args.first());
        if (fi.isDir())
            opts.folder = fi.absoluteFilePath();
        else if (fi.suffix().compare("gcproj", Qt::CaseInsensitive) == 0)
            opts.project = fi.absoluteFilePath();
    }

    MainWindow w;
    w.show();
    w.showMaximized();

    if (opts.hasWork())
    {
        w.applyCommandLine(opts);
        if (opts.quit)
            return 0;
    }

    return a.exec();
}

