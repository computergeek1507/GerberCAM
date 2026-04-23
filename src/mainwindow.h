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

#pragma once

#include <QMainWindow>
#include <QGraphicsScene>
#include <QLineF>
#include <QWheelEvent>
#include <qmath.h>
#include <qpainter.h>
#include <QColor>
#include <QTreeView>
#include <QTreeWidget>
#include <QSettings>
#include "treemodel.h"
#include "ui_mainwindow.h"
#include "ui_settingwindow.h"
#include "settingwindow.h"

#include "gerber.h"
#include "drawPCB.h"
#include "toolpath.h"
#include "preprocess.h"
#include "gcodeexport.h"
#include "excellonparser.h"

#include "clipper.hpp"
using namespace ClipperLib;

#include "config.h"

#include "spdlog/spdlog.h"
#include "spdlog/common.h"

#include <memory>

static auto version = PROJECT_NAME " v" PROJECT_VER;

namespace Ui {
class MainWindow;
}

namespace Ui {
class settingwindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void drawNet(QGraphicsScene *scene, Preprocess &t, QColor color, QColor colorError);
    void drawToolpath(QGraphicsScene *scene, Toolpath &t);
    void drawExcellonDrills(QGraphicsScene *scene);
    void showMessage(Gerber *g, Preprocess &p);
private slots:
    void on_actionOpen_triggered();

    void on_actionAdd_layer_triggered();

    void on_actionOpen_Outline_triggered();

    void on_actionOpen_Excellon_triggered();

    void on_actionSetting_triggered();

    void on_actionExit_triggered();

    void on_actionZoom_in_triggered();

    void on_actionZoom_out_triggered();

    void on_actionLayer1_triggered();

    void on_actionLayer2_triggered();

    void on_actionFlip_Board_triggered();

    void on_actionToolpath_generat_triggered();

    void on_actionExport_GCode_triggered();

    void on_actionExport_Drills_triggered();

    void on_actionExport_Drill_G_Code_Bore_triggered();

    void on_actionExport_Drills_Excellon_triggered();

    void on_actionExport_Drills_Excellon_Bore_triggered();

    void on_actionExport_Outline_triggered();

    void on_actionAbout_GerberCAM_triggered();

    void on_actionView_Log_triggered();

private:
    Ui::MainWindow *ui;

    double scale = 1.0;
    void wheelEvent(QWheelEvent *event);
    void drawLayer(QGraphicsScene *scene, Gerber *gerberfile, QColor color);
    void timerEvent(QTimerEvent* event);

    double scaleFactor = 250.0;
    int layerNum = 0;
    int currentLayer = 1;

    std::unique_ptr<Gerber> gerber1{ nullptr };
    std::unique_ptr<Gerber> gerber2{ nullptr };

    std::unique_ptr<QGraphicsScene> scene1{ nullptr };
    std::unique_ptr<QGraphicsScene> scene12{ nullptr };
    std::unique_ptr<QGraphicsScene> scene2{ nullptr };
    std::unique_ptr<QGraphicsScene> scene21{ nullptr };
    std::unique_ptr<QGraphicsScene> sceneNet1{ nullptr };
    std::unique_ptr<QGraphicsScene> sceneNet2{ nullptr };
    std::unique_ptr<QGraphicsScene> sceneNet12{ nullptr };
    std::unique_ptr<QGraphicsScene> sceneNet21{ nullptr };
    std::unique_ptr<QGraphicsScene> scenePath1{ nullptr };
    std::unique_ptr<QGraphicsScene> scenePath2{ nullptr };
    std::unique_ptr<QGraphicsScene> scenePath12{ nullptr };
    std::unique_ptr<QGraphicsScene> scenePath21{ nullptr };

    std::unique_ptr<QColor> colorRed1 = std::make_unique<QColor>(30,144,225,240);
    std::unique_ptr<QColor> colorRed2 = std::make_unique<QColor>(30,144,225,20);
    std::unique_ptr<QColor> colorBlue1 = std::make_unique<QColor>(30,144,225,240);
    std::unique_ptr<QColor> colorBlue2 = std::make_unique<QColor>(30,144,225,20);
    std::unique_ptr<QColor> Error1 = std::make_unique<QColor>(255,0,0,230);
    std::unique_ptr<QColor> Error2 = std::make_unique<QColor>(255,0,0,40);

    QLabel *coordinateLabel;

    std::unique_ptr <Preprocess> preprocessfile1{ nullptr };
    std::unique_ptr <Preprocess> preprocessfile2{ nullptr };
    std::unique_ptr<ExcellonParser> m_excellon{ nullptr };
    std::unique_ptr<Toolpath> toolpath1{ nullptr };
    std::unique_ptr<Toolpath> toolpath2{ nullptr };

    QString gerberFileName;

    bool recalculateFlag = false;
    bool boardFlipped = false;

    std::unique_ptr<Settingwindow> settingWindow{ nullptr };

    QString alertHtml = "<font color=\"red\">";
    QString endHtml = "</font>";

    std::shared_ptr<spdlog::logger> m_logger{ nullptr };
    QString m_appdir;

    std::unique_ptr<Gerber> gerberOutline{ nullptr };
    std::unique_ptr<QGraphicsScene> sceneOutline{ nullptr };
    std::unique_ptr<QColor> colorOutline = std::make_unique<QColor>(0, 200, 0, 200);
};
