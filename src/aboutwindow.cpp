#include "aboutwindow.h"
#include "ui_aboutwindow.h"
#include "config.h"

aboutwindow::aboutwindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::aboutwindow)
{
    ui->setupUi(this);
    ui->label_GerberCAM->setText(PROJECT_NAME " v" PROJECT_VER);
	ui->labelIcon->setPixmap(QPixmap(":/GerberCAM.ico").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    //ui->label_Link->setText("<a href=\"http://lichaoma.com/2015/11/14/gerbercam-a-pcb-tool-path-generator\" >Opensource PCB G-code Generator</a>");
    //ui->label_Link->setOpenExternalLinks(true);
}

aboutwindow::~aboutwindow()
{
    delete ui;
}
