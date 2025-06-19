#include "aboutwidget.h"
#include "ui_aboutwidget.h"
#include <QFile>
#include <QCommandLineParser>
#include "mainwindow.h"

aboutWidget::aboutWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::aboutWidget)
{
    ui->setupUi(this);
    ui->helpView->showPdf("D:\\Desktop\\test.pdf");
}

aboutWidget::~aboutWidget()
{
    delete ui;
}

void aboutWidget::initThis(){

}


