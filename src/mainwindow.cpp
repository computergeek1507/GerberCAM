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
#include "toolpath.h"
#include "aboutwindow.h"


#include <QFileDialog>
#include <QMessageBox>
#include <QSet>
#include <QTimer>
using namespace ClipperLib;

#include "scale.h"

#include "spdlog/sinks/qt_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <filesystem>


/*
 *The main GUI function.It contains mainly three parts:
 *  -Menu and toolbar on the top
 *  -Message and Info about the file on the left
 *  -A graphicsView of the file on the right
 * */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    QCoreApplication::setApplicationName(PROJECT_NAME);
    QCoreApplication::setApplicationVersion(PROJECT_VER);
    auto const log_name{ "log.txt" };

    m_appdir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::filesystem::create_directory(m_appdir.toStdString());
    QString logdir = m_appdir + "/log/";
    std::filesystem::create_directory(logdir.toStdString());

    try
    {
        auto file{ std::string(logdir.toStdString() + log_name) };
        auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file, 1024 * 1024, 5, false);

        m_logger = std::make_shared<spdlog::logger>(PROJECT_NAME, rotating);
        m_logger->flush_on(spdlog::level::debug);
        m_logger->set_level(spdlog::level::debug);
        m_logger->set_pattern("[%D %H:%M:%S] [%L] %v");
        spdlog::register_logger(m_logger);
    }
    catch (std::exception& /*ex*/)
    {
        QMessageBox::warning(this, "Logger Failed", "Logger Failed To Start.");
    }

	settingWindow = std::make_unique<Settingwindow>(m_appdir, this);
    ui->setupUi(this);
    /*
     * Initialize the graphicsview to show the gerber file.
     * A matrix is used to auto scale the file and show it properly.
     * Allow press and drag to view the gerber file.
     * Allow mouse wheele to zoom in and out the view.
     * */
    //QMatrix matrix;
    //matrix.scale(1, -1);
    //ui->graphicsView->setMatrix(matrix);
    // Make the splitter the central widget directly so the QMainWindow
    // manages its geometry — bypasses the intermediate QHBoxLayout which
    // was preventing the splitter children from resizing.
    setCentralWidget(ui->splitter);

    ui->splitter->setHandleWidth(6);
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 3);
    //ui->LayerTab1->setMinimumWidth(50);
    //ui->graphicsView->setMinimumWidth(50);

   // QTimer::singleShot(0, this, [this]() {
    //    ui->splitter->setSizes({ 280, ui->splitter->width() - 280 });
   // });

    ui->graphicsView->scale(1,-1);
    ui->graphicsView->setInteractive(true);
    //ui->graphicsView->setRenderHint(QPainter::Antialiasing, true);
    ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->graphicsView->setOptimizationFlags(QGraphicsView::DontSavePainterState);
    ui->graphicsView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    ui->graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    ui->statusBar->showMessage(tr("Ready"));

    /*
     * Set up the coordinateLabel at the bottom.
     * */
    coordinateLabel= new QLabel(this);
    //coordinateLabel->setAlignment(Qt::AlignLeft);
    ui->statusBar->addPermanentWidget(coordinateLabel);

    /*
     * Initialize the panel on the left side.
     * */
    ui->LayerTab1->setTabText(0,"Message");
    ui->LayerTab1->setTabText(1,"Layer1");
    ui->LayerTab1->setTabText(2,"Layer2");
    ui->LayerTab1->setTabText(3,"Outline");
    //ui->LayerTab1->setFixedWidth(350);


    //ui->statusBar->addPermanentWidget();

    this->setWindowTitle(version);

    /*
     * What am I doing here???
     * */
    Path subj;
    Paths solution;
    subj <<
    IntPoint(348,257) << IntPoint(364,148) << IntPoint(362,148) <<
    IntPoint(326,241) << IntPoint(295,219) << IntPoint(258,88) <<
    IntPoint(440,129) << IntPoint(370,196) << IntPoint(372,275);
    ClipperOffset co;
    co.AddPath(subj, jtRound, etClosedPolygon);
    co.Execute(solution, -7.0);

    startTimer(50);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::drawNet(QGraphicsScene* scene, Preprocess& t, QColor color, QColor colorError)
{
    if (t.netList.size() == 0)
    {
        return;
    }

    for(int i=0;i<t.netList.size();i++)
    {
        if(t.netList.at(i).elements.size()==0) continue;
        /*
        if(i*2<t.netList.size())
            color.setBlue((255*(i+1)*2/t.netList.size())%256);
        else
            color.setGreen((255*((i+1)-t.netList.size()/2)*2/t.netList.size())%256);
        */
        for (int j = 0; j < t.netList.at(i).elements.size(); j++)//draw track first
        {
            QColor c = color;
            if (t.netList.at(i).collisionFlag )
            {
                c = colorError;
            }

            if(t.netList.at(i).elements.at(j).elementType=='T')
            {
                QGraphicsItem *item = new DrawPCB(t.netList.at(i).elements.at(j).track,'T', AT_TOP,c);
                item->setPos(t.netList.at(i).elements.at(j).track.pointstart);
                scene->addItem(item);
            }
        }
        for (int j = 0; j < t.netList.at(i).elements.size(); j++)//draw pad without hole
        {
            QColor c = color;
            if (t.netList.at(i).collisionFlag)
            {
                c = colorError;
            }
            if(t.netList.at(i).elements.at(j).elementType=='P')
            {
                double x1=t.netList.at(i).elements.at(j).pad.point.x();
                double y1=t.netList.at(i).elements.at(j).pad.point.y();

                QGraphicsItem *item = new DrawPCB(t.netList.at(i).elements.at(j).pad, AT_TOP,c);
                item->setPos(x1,y1);
                scene->addItem(item);
            }
        }

        for(int j=0;j<t.netList.at(i).elements.size();j++)//draw pad with hole
        {
            QColor c = color;

            if(t.netList.at(i).elements.at(j).elementType=='P'&&
                    t.netList.at(i).elements.at(j).pad.hole!=0)
            {
                double x1=t.netList.at(i).elements.at(j).pad.point.x();
                double y1=t.netList.at(i).elements.at(j).pad.point.y();

                QGraphicsItem *item = new DrawPCB(t.netList.at(i).elements.at(j).pad,'h', AT_TOP,c);
                item->setPos(x1,y1);
                scene->addItem(item);
            }
        }
    }
    for(int i=0;i<t.contourList.size();i++)
    {
        Net n = t.contourList.at(i);
        for(int j = 0; j<n.elements.size(); j++)
        {
            Track tempTrack=n.elements.at(j).track;
            QGraphicsItem *item = new DrawPCB(tempTrack,'C', AT_TOP,Qt::black);
            item->setPos(tempTrack.pointstart);
            scene->addItem(item);
        }
    }
}

void MainWindow::drawToolpath(QGraphicsScene *scene,Toolpath &t)
{
    /*
    int i;
    for(i=0;i<t.netPathList.size();i++)
    {
        int j;
        QColor color(Qt::cyan);

        struct NetPath np=t.netPathList.at(i);

        for(j=0;j<np.pathList.size();j++)
        {
            QPoint point;

            struct MyPath p=np.pathList.at(j);
            QGraphicsItem *item = new DrawPCB(p, AT_TOP,color);

            //Paths p=np.toolpath;
            //QGraphicsItem *item=new drawPCB(p,AT_TOP,color);
            //point.setX(p.at(0).at(0).X);
            //point.setY(p.at(0).at(0).Y);
            //item->setPos(point);

            item->setPos(p.segmentList.at(0).point);
            scene->addItem(item);
        }
    }
    */

    //m_logger->debug("drawToolpath: totalToolpath paths={}", t.totalToolpath.size());
    if(t.totalToolpath.empty() || t.totalToolpath.at(0).empty())
    {
        //m_logger->debug("drawToolpath: totalToolpath empty, nothing to draw");
        return;
    }
    QColor color(255,170,32);
    //color.setGreen(150);
    Paths p=t.totalToolpath;
    QGraphicsItem *item=new DrawPCB(p,AT_TOP,color);
    QPoint point;
    point.setX(p.at(0).at(0).X);
    point.setY(p.at(0).at(0).Y);
    item->setPos(point);
    scene->addItem(item);
    //m_logger->debug("drawToolpath: item added at ({}, {}), scene items={}", point.x(), point.y(), scene->items().size());
}

void MainWindow::drawExcellonDrills(QGraphicsScene* scene)
{
    if (!m_excellon || !scene) {
        return;
    }

    int totalHoles = 0;
    for (const ExcellonTool& t : m_excellon->tools) {
        totalHoles += t.holes.size();
    }

    if (totalHoles == 0) {
        return;
    }

    // Crosshair length: 2% of the scene diagonal so it is clearly
    // visible at board-level zoom regardless of board size.
    const QRectF bounds = scene->sceneRect();
    double diag  = std::sqrt(bounds.width()  * bounds.width()
                           + bounds.height() * bounds.height());
    double crossLen = diag * 0.02;   // 2% of board diagonal

    // Cosmetic pen: always 1 pixel wide on screen regardless of zoom.
    QPen circlePen(QColor(255, 220, 0));
    circlePen.setWidth(0);
    QBrush circleFill(QColor(255, 220, 0, 120));

    QPen crossPen(QColor(255, 220, 0));
    crossPen.setWidth(0);

    for (const ExcellonTool& tool : m_excellon->tools)
    {
        if (tool.diameterMm <= 0.0) {
            continue;
        }

        double r = (tool.diameterMm * PRECISIONSCALE) / 2.0;
        // Crosshair is at least 3× the drill radius so it is legible even
        // when zoomed in tight.
        double arm = std::max(crossLen, r * 3.0);

        for (const QPoint &hole : tool.holes)
        {
            double cx = hole.x(), cy = hole.y();

            // Circle — placed directly in scene coordinates (no setPos).
            auto *circle = scene->addEllipse(cx - r, cy - r, r * 2.0, r * 2.0,
                                             circlePen, circleFill);
            circle->setZValue(100);

            // Crosshair lines — also in absolute scene coordinates.
            auto *h = scene->addLine(cx - arm, cy, cx + arm, cy, crossPen);
            auto *v = scene->addLine(cx, cy - arm, cx, cy + arm, crossPen);
            h->setZValue(100);
            v->setZValue(100);
        }
    }
}

void MainWindow::drawLayer(QGraphicsScene *scene,Gerber *gerberfile,QColor color)
{
    //Track tempTrack;
    for(int i=0;i<gerberfile->trackNum;i++)
    {
        Track tempTrack = gerberfile->tracksList.at(i);
        QGraphicsItem *item = new DrawPCB(tempTrack,'T', AT_TOP,color);
        item->setPos(tempTrack.pointstart);
        scene->addItem(item);
    }

    for(int i=0;i<gerberfile->padNum;i++)
    {
        Pad tempPad=gerberfile->padsList.at(i);
        double x1=tempPad.point.rx();
        double y1=tempPad.point.ry();

        QGraphicsItem *item = new DrawPCB(tempPad, AT_TOP,color);
        item->setPos(x1,y1);
        scene->addItem(item);
    }
    //scene->addRect(gerberfile->borderRect);
}

void MainWindow::showMessage(Gerber *g,Preprocess &p)
{
    setWindowTitle(gerberFileName + " - " + version);
    ui->messageBrowser->append("\"" + gerberFileName + "\"" + " read success");
    ui->messageBrowser->append("   Total line         =" + QString::number(g->totalLine));
    ui->messageBrowser->append("   Preprocessing time =" + QString::number(p.time) + "ms");
    ui->messageBrowser->append("   Contour number     =" + QString::number(p.contourList.size()));
    ui->messageBrowser->append("   Pad number         =" + QString::number(g->padNum));
    ui->messageBrowser->append("   Track number       =" + QString::number(p.elementList.size()-g->padNum));
    ui->messageBrowser->append("   Net number         =" + QString::number(p.netList.size()));
}

void MainWindow::on_actionOpen_triggered()
{
    auto const fileName = QFileDialog::getOpenFileName(this, tr("Open Gerber"), settingWindow->settings->lastDir(),
        tr("Top Layer (*.gtl);;Bottom Layer (*.gbl);;Gerber File(*.gbr *.gbl *gtl);;All types (*.*)"));
    if (fileName.isEmpty())
    {
        return;
    }
    settingWindow->settings->setLastDir(fileName);
    gerber1 = std::make_unique<Gerber>(fileName);
    gerberFileName = QFileInfo(fileName).fileName();
    if(!gerber1->readingFlag)
    {
        m_logger->error("\"" + gerberFileName.toStdString() + "\"" + " read fail");
        ui->messageBrowser->append("\""+gerberFileName+"\""+" read fail");
        ui->messageBrowser->append("Failed at line="+QString::number(gerber1->totalLine));
        return;
    }

    scene1 = std::make_unique<QGraphicsScene>(this);
    drawLayer(scene1.get(), gerber1.get(), *colorRed1);

    ui->graphicsView->setScene(scene1.get());
    ui->graphicsView->fitInView(gerber1->borderRect, Qt::KeepAspectRatio);
    //qDebug()<<gerber1->borderRect;

    layerNum = 1;
    currentLayer = 1;
    ui->actionLayer2->setEnabled(false);
    ui->actionAdd_layer->setEnabled(true);
    ui->actionToolpath_generat->setEnabled(true);
    ui->actionExport_Drills->setEnabled(true);
    ui->actionExport_Drill_G_Code_Bore->setEnabled(true);

    preprocessfile1 = std::make_unique<Preprocess>(*gerber1, settingWindow->settings);

    ui->messageBrowser->clear();
    showMessage(gerber1.get(),*preprocessfile1);

    sceneNet1 = std::make_unique<QGraphicsScene>(this);
    drawNet(sceneNet1.get(),*preprocessfile1,*colorBlue1,*Error1);
    drawExcellonDrills(sceneNet1.get());
    ui->graphicsView->setScene(sceneNet1.get());

    TreeModel *model = new TreeModel(*preprocessfile1);

    ui->treeViewlayer1->setModel(model);
    ui->treeViewlayer1->setColumnWidth(0,200);
    ui->treeViewlayer1->show();

    recalculateFlag = true;
}

void MainWindow::on_actionAdd_layer_triggered()
{
    if (layerNum == 1)//no any layer2,draw a new layer2
    {
        auto const fileName = QFileDialog::getOpenFileName(this, tr("Open Gerber"), settingWindow->settings->lastDir(),
            tr("Bottom Layer (*.gbl);;Top Layer(*.gtl);;Gerber Files (*.gbr *.gbl *gtl);;All types (*.*)"));
        if (fileName.isEmpty())
        {
            return;
        }
        settingWindow->settings->setLastDir(fileName);
        gerberFileName = QFileInfo(fileName).fileName();
        gerber2 = std::make_unique<Gerber>(fileName);
        if (!gerber2->readingFlag)
        {
            m_logger->error("\"" + gerberFileName.toStdString() + "\"" + " read fail");
            ui->messageBrowser->append("\"" + gerberFileName + "\"" + " read fail");
            ui->messageBrowser->append("Failed at line=" + QString::number(gerber2->totalLine));
            return;
        }
        preprocessfile2 = std::make_unique<Preprocess>(*gerber2, settingWindow->settings);

        showMessage(gerber2.get(), *preprocessfile2);

        //ui->graphicsView->setScene(scene21);
        layerNum = 2;
        currentLayer = 2;
        ui->actionLayer2->setEnabled(true);

        TreeModel* model = new TreeModel(*preprocessfile2);

        ui->treeViewlayer2->setModel(model);
        ui->treeViewlayer2->setColumnWidth(0, 200);
        ui->treeViewlayer2->show();

    }
    else if (currentLayer == 2)//add to layer2
    {
        auto const fileName = QFileDialog::getOpenFileName(this, tr("Open Gerber"), settingWindow->settings->lastDir(),
            tr("Bottom Layer (*.gbl);;Top Layer(*.gtl);;Gerber Files (*.gbr *.gbl *gtl);;All types (*.*)"));
        if (fileName.isEmpty())
        {
            return;
        }
        settingWindow->settings->setLastDir(fileName);

        gerber2 = std::make_unique<Gerber>(fileName);
        if (!gerber2->readingFlag)
        {
            m_logger->error("\"" + gerberFileName.toStdString() + "\"" + " read fail");
            ui->messageBrowser->append("\"" + gerberFileName + "\"" + " read fail");
            ui->messageBrowser->append("Failed at line=" + QString::number(gerber2->totalLine));
            return;
        }
        preprocessfile2 = std::make_unique<Preprocess>(*gerber2, settingWindow->settings);

        showMessage(gerber2.get(), *preprocessfile2);

        TreeModel* model = new TreeModel(*preprocessfile2);

        ui->treeViewlayer2->setModel(model);
        ui->treeViewlayer2->setColumnWidth(0, 200);
        ui->treeViewlayer2->show();

        //ui->graphicsView->setScene(scene21);
    }
    else if (currentLayer == 1)//add to layer1
    {
        auto const fileName = QFileDialog::getOpenFileName(this, tr("Open Gerber"), settingWindow->settings->lastDir(),
            tr("Top Layer(*.gtl);;Bottom Layer (*.gbl);;Gerber Files (*.gbr *.gbl *gtl);;All types (*.*)"));
        if (fileName.isEmpty())
        {
            return;
        }
        settingWindow->settings->setLastDir(fileName);

        gerber1 = std::make_unique<Gerber>(fileName);
        if (!gerber1->readingFlag)
        {
            m_logger->error("\"" + gerberFileName.toStdString() + "\"" + " read fail");
            ui->messageBrowser->append("\"" + gerberFileName + "\"" + " read fail");
            ui->messageBrowser->append("Failed at line=" + QString::number(gerber1->totalLine));
            return;
        }
        preprocessfile1 = std::make_unique<Preprocess>(*gerber1, settingWindow->settings);

        showMessage(gerber1.get(), *preprocessfile1);

        TreeModel* model = new TreeModel(*preprocessfile1);

        ui->treeViewlayer1->setModel(model);
        ui->treeViewlayer1->setColumnWidth(0, 200);
        ui->treeViewlayer1->show();
        //ui->graphicsView->setScene(scene12);
    }

    preprocessfile1->clearEccentricHole(gerber2->padsList);
    preprocessfile2->clearEccentricHole(gerber1->padsList);

    sceneNet1 = std::make_unique<QGraphicsScene>(this);
    drawNet(sceneNet1.get(), *preprocessfile1, *colorRed1, *Error1);
    drawExcellonDrills(sceneNet1.get());

    sceneNet2 = std::make_unique<QGraphicsScene>(this);
    drawNet(sceneNet2.get(), *preprocessfile2, *colorBlue1, *Error1);
    drawExcellonDrills(sceneNet2.get());

    sceneNet21 = std::make_unique<QGraphicsScene>(this);
    drawNet(sceneNet21.get(), *preprocessfile1, *colorRed2, *Error2);
    drawNet(sceneNet21.get(), *preprocessfile2, *colorBlue1, *Error1);
    drawExcellonDrills(sceneNet21.get());

    sceneNet12 = std::make_unique<QGraphicsScene>(this);
    drawNet(sceneNet12.get(), *preprocessfile2, *colorBlue2, *Error2);
    drawNet(sceneNet12.get(), *preprocessfile1, *colorRed1, *Error1);
    drawExcellonDrills(sceneNet12.get());

    if (currentLayer == 1) {
        ui->graphicsView->setScene(sceneNet12.get());
    }
    else if (currentLayer == 2) {
        ui->graphicsView->setScene(sceneNet21.get());
    }
    recalculateFlag = true;
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionToolpath_generat_triggered()
{
    toolpath1 = std::make_unique<Toolpath>(preprocessfile1.get(), settingWindow->settings, settingWindow->settings->engravingParm);

    if (layerNum == 2)
    {
        toolpath2 = std::make_unique<Toolpath>(preprocessfile2.get(), settingWindow->settings, settingWindow->settings->engravingParm);
        scenePath12 = std::make_unique<QGraphicsScene>(this);
        drawNet(scenePath12.get(), *preprocessfile2, *colorBlue2, *Error2);
        drawNet(scenePath12.get(), *preprocessfile1, *colorRed1, *Error1);
        drawToolpath(scenePath12.get(), *toolpath1);
        if (gerberOutline) drawLayer(scenePath12.get(), gerberOutline.get(), *colorOutline);
        drawExcellonDrills(scenePath12.get());
        scenePath21 = std::make_unique<QGraphicsScene>(this);
        drawNet(scenePath21.get(), *preprocessfile1, *colorRed2, *Error2);
        drawNet(scenePath21.get(), *preprocessfile2, *colorBlue1, *Error1);
        drawToolpath(scenePath21.get(), *toolpath2);
        if (gerberOutline) drawLayer(scenePath21.get(), gerberOutline.get(), *colorOutline);
        drawExcellonDrills(scenePath21.get());

        QString temp = alertHtml + QString::number(toolpath1->collisionSum) + endHtml;
        ui->messageBrowser->append("Layer1 Toolpath collision=" + temp);
        ui->messageBrowser->append("   Calculation time   =" + QString::number(toolpath1->time) + "ms");
        temp = alertHtml + QString::number(toolpath2->collisionSum) + endHtml;
        ui->messageBrowser->append("Layer2 Toolpath collision=" + temp);
        ui->messageBrowser->append("   Calculation time   =" + QString::number(toolpath2->time) + "ms");
    }
    else
    {
        scenePath1 = std::make_unique<QGraphicsScene>(this);
        drawNet(scenePath1.get(), *preprocessfile1, *colorBlue1, *Error1);
        drawToolpath(scenePath1.get(), *toolpath1);
        if (gerberOutline) drawLayer(scenePath1.get(), gerberOutline.get(), *colorOutline);
        drawExcellonDrills(scenePath1.get());
        QString temp = alertHtml + QString::number(toolpath1->collisionSum) + endHtml;
        ui->messageBrowser->append("Layer1 Toolpath collision=" + temp);
        ui->messageBrowser->append("   Calculation time   =" + QString::number(toolpath1->time) + "ms");
    }

    recalculateFlag = false;
    ui->actionExport_GCode->setEnabled(true);

    if (layerNum == 2)
    {
        if (currentLayer == 1) {
            ui->graphicsView->setScene(scenePath12.get());
        }
        else {
            ui->graphicsView->setScene(scenePath21.get());
        }
    }
    else {
        ui->graphicsView->setScene(scenePath1.get());
    }
}

void MainWindow::on_actionExport_GCode_triggered()
{
    Toolpath* tp = nullptr;
    if (layerNum == 2) {
        tp = (currentLayer == 1) ? toolpath1.get() : toolpath2.get();
    }
    else {
        tp = toolpath1.get();
    }

    if (!tp || tp->totalToolpath.empty())
    {
        QMessageBox::warning(this, "Export G-Code",
            "No toolpath available. Run Toolpath Generate first.");
        return;
    }

    QString defaultName = gerberFileName;
    if (!defaultName.isEmpty())
    {
        int dot = defaultName.lastIndexOf('.');
        if (dot >= 0) { defaultName.truncate(dot); }
        defaultName += ".nc";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export G-Code", settingWindow->settings->lastDir() + "/" + defaultName,
        "G-Code files (*.nc *.gcode *.tap);;All files (*)");

    if (filePath.isEmpty())
        return;

    settingWindow->settings->setLastDir(filePath);

    QString errorMsg;
    if (GcodeExport::write(*tp, *settingWindow->settings, filePath, errorMsg, boardFlipped))
    {
        ui->messageBrowser->append("G-Code exported: " + QFileInfo(filePath).fileName());
        ui->messageBrowser->append("  Paths: " + QString::number(tp->totalToolpath.size()));
        m_logger->info("G-Code exported: {}", filePath.toStdString());
    }
    else
    {
        m_logger->error("Export G-Code failed: {}", errorMsg.toStdString());
        QMessageBox::critical(this, "Export G-Code", errorMsg);
    }
}

void MainWindow::on_actionZoom_in_triggered()
{
    ui->graphicsView->scale(1.125,1.125);
}

void MainWindow::on_actionZoom_out_triggered()
{
    ui->graphicsView->scale(0.888889,0.888889);
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    // not sure about this change though
    //double numDegrees = event->delta() / 8.0;
    double const numDegrees = event->angleDelta().y() / 8.0;
    double const numSteps = numDegrees / 15.0;
    double const factor = pow(1.125, numSteps);
    ui->graphicsView->scale(factor,factor);
}

void MainWindow::on_actionLayer1_triggered()
{
    currentLayer = 1;
    if(recalculateFlag) {
        if (layerNum == 1) {
            ui->graphicsView->setScene(sceneNet1.get());
        }
        else {
            ui->graphicsView->setScene(sceneNet12.get());
        }
    }
    else {
        if (layerNum == 2) {
            ui->graphicsView->setScene(scenePath12.get());
        }
        else {
            ui->graphicsView->setScene(scenePath1.get());
        }
    }
}

void MainWindow::on_actionLayer2_triggered()
{
    currentLayer = 2;
    if (recalculateFlag) {
        ui->graphicsView->setScene(sceneNet21.get());
    }
    else {
        ui->graphicsView->setScene(scenePath21.get());
    }
}

void MainWindow::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event)
    QPoint const p1 = QCursor::pos();
    QPoint const p1_mapped = ui->graphicsView->mapFromGlobal(p1);
    QPointF const p2 = ui->graphicsView->mapToScene(p1_mapped);
    QString const s = "(" + QString::number(p2.x() / PRECISIONSCALE, 'f', 3) + "," + QString::number(p2.y() / PRECISIONSCALE, 'f', 3) + ") mm  ";
    coordinateLabel->setText(s);
}

void MainWindow::on_actionFlip_Board_triggered()
{
    boardFlipped = ui->actionFlip_Board->isChecked();
    // Multiply the current view transform by scale(-1, 1) to toggle horizontal mirror.
    // Calling this twice returns to the original orientation.
    ui->graphicsView->scale(-1, 1);
    ui->messageBrowser->append(boardFlipped ? "Board flipped (X mirrored)"
                                            : "Board unflipped");
}

void MainWindow::on_actionExport_Drills_triggered()
{
    if(!preprocessfile1)
    {
        QMessageBox::warning(this, "Export Drill G-Code",
                             "No file loaded. Open a Gerber file first.");
        return;
    }

    QString defaultName = gerberFileName;
    if(!defaultName.isEmpty())
    {
        int dot = defaultName.lastIndexOf('.');
        if(dot >= 0) defaultName.truncate(dot);
        defaultName += "_drill.nc";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export Drill G-Code", settingWindow->settings->lastDir() + "/" + defaultName,
        "G-Code files (*.nc *.gcode *.tap);;All files (*)");

    if(filePath.isEmpty())
    {
        return;
    }
    settingWindow->settings->setLastDir(filePath);

    // Use layer 1 — clearEccentricHole already removed non-through-holes
    QString errorMsg;
    if(GcodeExport::writeDrills(*preprocessfile1, *settingWindow->settings,
                                filePath, errorMsg, boardFlipped))
    {
        // Count holes for the status message
        int total = 0;
        QSet<qint64> diams;
        for(const Net &n : preprocessfile1->netList)
            for(const Element &e : n.elements)
                if(e.elementType == 'P' && e.pad.hole > 0)
                { ++total; diams.insert(e.pad.hole); }

        ui->messageBrowser->append("Drill G-Code exported: " + filePath);
        m_logger->info("Drill G-Code exported: {}", filePath.toStdString());
        ui->messageBrowser->append("  Holes: " + QString::number(total)
                                   + ", Diameters: " + QString::number(diams.size()));
    }
    else
    {
        m_logger->error("Export Drill G-Code failed: {}", errorMsg.toStdString());
        QMessageBox::critical(this, "Export Drill G-Code", errorMsg);
    }
}

void MainWindow::on_actionExport_Drill_G_Code_Bore_triggered() 
{
    if (!preprocessfile1)
    {
        QMessageBox::warning(this, "Export Drill G-Code",
            "No file loaded. Open a Gerber file first.");
        return;
    }

	QString defaultName = gerberFileName;

    if (!defaultName.isEmpty())
    {
        int dot = defaultName.lastIndexOf('.');
        if (dot >= 0) defaultName.truncate(dot);
        defaultName += "_drill_bore.nc";
    }
    else
    {
        defaultName = "bore.nc";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export Drill G-Code (Bore)",
        settingWindow->settings->lastDir() + "/" + defaultName,
        "G-Code files (*.nc *.gcode *.tap);;All files (*)");

    if (filePath.isEmpty())
        return;

    settingWindow->settings->setLastDir(filePath);

    QString errorMsg;
    if (GcodeExport::writeDrillsBore(*preprocessfile1, *settingWindow->settings,
        filePath, errorMsg, boardFlipped))
    {
        int total = 0;
        QSet<qint64> diams;
        for (const Net& n : preprocessfile1->netList)
            for (const Element& e : n.elements)
                if (e.elementType == 'P' && e.pad.hole > 0)
                {
                    ++total; diams.insert(e.pad.hole);
                }


        ui->messageBrowser->append("Drill Bore G-Code exported: " + QFileInfo(filePath).fileName());
        ui->messageBrowser->append("  Tools: " + QString::number(m_excellon->tools.size())
            + ", Holes: " + QString::number(total));
        m_logger->info("Drill Bore G-Code exported: {}", filePath.toStdString());
    }
    else
    {
        m_logger->error("Export Drill Bore G-Code failed: {}", errorMsg.toStdString());
        QMessageBox::critical(this, "Export Drill G-Code (Bore)", errorMsg);
    }
}

void MainWindow::on_actionSetting_triggered()
{
    //settingwindow s;
    settingWindow->setModal(true);
    settingWindow->exec();
}

void MainWindow::on_actionAbout_GerberCAM_triggered()
{
    aboutwindow about;
    about.setModal(true);
    about.exec();
}

void  MainWindow::on_actionView_Log_triggered()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_appdir + "/log/"));
}

void MainWindow::on_actionOpen_Outline_triggered()
{
    auto const fileName = QFileDialog::getOpenFileName(this, tr("Open Edge Cut / Outline"), settingWindow->settings->lastDir(),
        tr("Edge Cuts (*.gm1 *.gko *.gm);;Gerber Files (*.gbr *.ger);;All types (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }
    settingWindow->settings->setLastDir(fileName);

    gerberOutline = std::make_unique<Gerber>(fileName);
    if (!gerberOutline->readingFlag)
    {
        m_logger->error("Outline read fail: {}", QFileInfo(fileName).fileName().toStdString());
        ui->messageBrowser->append("Outline file read fail");
        return;
    }

    // Draw the outline onto every existing copper / net scene so it
    // overlays correctly regardless of which view is active.
    QList<QGraphicsScene*> scenes;
    for (auto* s : { sceneNet1.get(), sceneNet2.get(), sceneNet12.get(), sceneNet21.get(),
                    scenePath1.get(), scenePath2.get(), scenePath12.get(), scenePath21.get() })
    {
        if (s)
        {
            scenes.append(s);
        }
    }
    for (auto* s : scenes)
    {
        if (s)
        {
            drawLayer(s, gerberOutline.get(), *colorOutline);
        }
    }

    // Also build a standalone outline-only scene
    if (sceneOutline) {
        sceneOutline.reset();
    }
    sceneOutline = std::make_unique<QGraphicsScene>(this);
    drawLayer(sceneOutline.get(), gerberOutline.get(), *colorOutline);

    // Show the current scene (with outline now added) or the standalone scene
    QGraphicsScene *current = ui->graphicsView->scene();
    if(current && scenes.contains(current))
    {
        // Already showing a copper scene - outline was just added to it, refresh
        ui->graphicsView->viewport()->update();
    }
    else
    {
        ui->graphicsView->setScene(sceneOutline.get());
    }

    // Fit view to the outline extents
    ui->graphicsView->fitInView(gerberOutline->borderRect, Qt::KeepAspectRatio);

    // Show outline info in the Outline tab
    ui->outlineBrowser->clear();
    QString fn = QFileInfo(fileName).fileName();
    ui->outlineBrowser->append("Outline: " + fn);
    ui->outlineBrowser->append("  Pads:   " + QString::number(gerberOutline->padNum));
    ui->outlineBrowser->append("  Tracks: " + QString::number(gerberOutline->trackNum));

    ui->LayerTab1->setCurrentWidget(ui->tabOutline);
    ui->messageBrowser->append("Outline loaded: " + fn);
    ui->actionExport_Outline->setEnabled(true);
}


void MainWindow::on_actionExport_Outline_triggered()
{
    if (!gerberOutline)
    {
        QMessageBox::warning(this, "Export Outline G-Code",
            "No outline loaded. Open an edge cut file first.");
        return;
    }

    QString defaultName = gerberFileName;
    if (!defaultName.isEmpty())
    {
        int dot = defaultName.lastIndexOf('.');
        if (dot >= 0) defaultName.truncate(dot);
        defaultName += "_outline.nc";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export Outline G-Code", settingWindow->settings->lastDir() + "/" + defaultName,
        "G-Code files (*.nc *.gcode *.tap);;All files (*)");

    if (filePath.isEmpty())
    {
        return;
    }
    settingWindow->settings->setLastDir(filePath);

    QString errorMsg;
    if(GcodeExport::writeOutline(*gerberOutline, *settingWindow->settings,
                                 filePath, errorMsg, boardFlipped))
    {
        ui->messageBrowser->append("Outline G-Code exported: " + QFileInfo(filePath).fileName());
        ui->messageBrowser->append("  Segments: " + QString::number(gerberOutline->tracksList.size()));
        m_logger->info("Outline G-Code exported: {}", filePath.toStdString());
    }
    else
    {
        m_logger->error("Export Outline G-Code failed: {}", errorMsg.toStdString());
        QMessageBox::critical(this, "Export Outline G-Code", errorMsg);
    }
}

void MainWindow::on_actionOpen_Excellon_triggered()
{
    auto const fileName = QFileDialog::getOpenFileName(
        this, tr("Open Excellon Drill File"),
        settingWindow->settings->lastDir(),
        tr("Excellon Drill Files (*.drl *.exc *.xln *.ncd *.drill);;All files (*.*)"));

    if (fileName.isEmpty()) {
        return;
    }

    settingWindow->settings->setLastDir(fileName);

    auto exc = std::make_unique<ExcellonParser>();
    QString errorMsg;
    if (!exc->parse(fileName, errorMsg))
    {
        m_logger->error("Excellon parse failed: {}", errorMsg.toStdString());
        QMessageBox::critical(this, "Open Excellon", errorMsg);
        return;
    }

    m_excellon = std::move(exc);

    // Overlay onto every scene that already exists
    for (QGraphicsScene* s : { sceneNet1.get(), sceneNet2.get(), sceneNet12.get(), sceneNet21.get(),
                               scenePath1.get(), scenePath2.get(), scenePath12.get(), scenePath21.get() }) {
        drawExcellonDrills(s);
    }
    if (ui->graphicsView->scene()) {
        ui->graphicsView->viewport()->update();
    }

    // Populate the Drills tab
    ui->drillsTree->clear();
    int totalHoles = 0;
    for (const ExcellonTool &t : m_excellon->tools)
    {
        totalHoles += t.holes.size();
        auto *top = new QTreeWidgetItem(ui->drillsTree);
        top->setText(0, QString::number(t.diameterMm, 'f', 3) + " mm");
        top->setText(1, QString::number(t.holes.size()));
        for (const QPoint &hole : t.holes)
        {
            auto *child = new QTreeWidgetItem(top);
            double xMm = hole.x() / static_cast<double>(PRECISIONSCALE);
            double yMm = hole.y() / static_cast<double>(PRECISIONSCALE);
            child->setText(0, QString("X%1  Y%2")
                .arg(xMm, 0, 'f', 3)
                .arg(yMm, 0, 'f', 3));
        }
    }
    ui->drillsTree->resizeColumnToContents(0);
    ui->drillsTree->resizeColumnToContents(1);
    ui->LayerTab1->setCurrentWidget(ui->tabDrills);

    QString fn = QFileInfo(fileName).fileName();
    ui->messageBrowser->append("Excellon loaded: " + fn);
    ui->messageBrowser->append("  Tools: " + QString::number(m_excellon->tools.size())
                               + ", Holes: " + QString::number(totalHoles));

    m_logger->info("Excellon loaded: {} ({} tools, {} holes)",
                   fileName.toStdString(),
                   m_excellon->tools.size(), totalHoles);

    ui->actionExport_Drills_Excellon->setEnabled(true);
    ui->actionExport_Drills_Excellon_Bore->setEnabled(true);
}

void MainWindow::on_actionExport_Drills_Excellon_triggered()
{
    if (!m_excellon)
    {
        QMessageBox::warning(this, "Export Excellon Drill G-Code",
            "No Excellon file loaded. Open an Excellon drill file first.");
        return;
    }

    // Derive a default output filename from the first Gerber (or generic name)
    QString defaultName = gerberFileName;
    if (!defaultName.isEmpty())
    {
        int dot = defaultName.lastIndexOf('.');
        if (dot >= 0) { defaultName.truncate(dot); }
        defaultName += "_exc_drill.nc";
    }
    else
    {
        defaultName = "drill.nc";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export Excellon Drill G-Code",
        settingWindow->settings->lastDir() + "/" + defaultName,
        "G-Code files (*.nc *.gcode *.tap);;All files (*)");

    if (filePath.isEmpty()) {
        return;
    }

    settingWindow->settings->setLastDir(filePath);

    QString errorMsg;
    if (GcodeExport::writeDrills(*m_excellon, *settingWindow->settings,
        filePath, errorMsg, boardFlipped))
    {
        int totalHoles = 0;
        for (const ExcellonTool& t : m_excellon->tools) {
            totalHoles += t.holes.size();
        }

        ui->messageBrowser->append("Excellon Drill G-Code exported: " + QFileInfo(filePath).fileName());
        ui->messageBrowser->append("  Tools: " + QString::number(m_excellon->tools.size())
                                   + ", Holes: " + QString::number(totalHoles));
        m_logger->info("Excellon Drill G-Code exported: {}", filePath.toStdString());
    }
    else
    {
        m_logger->error("Export Excellon Drill G-Code failed: {}", errorMsg.toStdString());
        QMessageBox::critical(this, "Export Excellon Drill G-Code", errorMsg);
    }
}

void MainWindow::on_actionExport_Drills_Excellon_Bore_triggered()
{
    if (!m_excellon)
    {
        QMessageBox::warning(this, "Export Excellon Drill G-Code (Bore)",
            "No Excellon file loaded. Open an Excellon drill file first.");
        return;
    }

    QString defaultName = gerberFileName;
    if (!defaultName.isEmpty())
    {
        int dot = defaultName.lastIndexOf('.');
        if (dot >= 0) { defaultName.truncate(dot); }
        defaultName += "_exc_bore.nc";
    }
    else
    {
        defaultName = "bore.nc";
    }

    QString filePath = QFileDialog::getSaveFileName(
        this, "Export Excellon Drill G-Code (Bore)",
        settingWindow->settings->lastDir() + "/" + defaultName,
        "G-Code files (*.nc *.gcode *.tap);;All files (*)");

    if (filePath.isEmpty()) {
        return;
    }

    settingWindow->settings->setLastDir(filePath);

    QString errorMsg;
    if (GcodeExport::writeDrillsBore(*m_excellon, *settingWindow->settings,
        filePath, errorMsg, boardFlipped))
    {
        int totalHoles = 0;
        for (const ExcellonTool& t : m_excellon->tools) {
            totalHoles += t.holes.size();
        }

        ui->messageBrowser->append("Excellon Bore G-Code exported: " + QFileInfo(filePath).fileName());
        ui->messageBrowser->append("  Tools: " + QString::number(m_excellon->tools.size())
                                   + ", Holes: " + QString::number(totalHoles));
        m_logger->info("Excellon Bore G-Code exported: {}", filePath.toStdString());
    }
    else
    {
        m_logger->error("Export Excellon Bore G-Code failed: {}", errorMsg.toStdString());
        QMessageBox::critical(this, "Export Excellon Drill G-Code (Bore)", errorMsg);
    }
}
