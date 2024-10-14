#include "aboutwidget.h"
#include "ui_aboutwidget.h"
#include <QFile>

aboutWidget::aboutWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::aboutWidget)
{
    ui->setupUi(this);
    QString helpHtml = "D:/AppFile/Person/附件/附件3：学业奖学金申请表（tanglifa）.doc";
    QFile file(helpHtml);
    if(file.open(QIODevice::ReadOnly|QIODevice::Text)){
        QString text = file.readAll();
        ui->helpBrowser->setHtml(text);
    }


}

aboutWidget::~aboutWidget()
{
    delete ui;
}
