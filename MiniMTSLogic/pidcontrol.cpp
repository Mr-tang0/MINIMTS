#include "pidcontrol.h"
#include "ui_pidcontrol.h"
#include <QDebug>
#include <QLineEdit>

PIDControl::PIDControl(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PIDControl)
{
    ui->setupUi(this);
}

void PIDControl::initPID(QMap<QString, QString> systemMap){
    if(!systemMap.empty())
    {
        QList<QLineEdit*> lineEdits = this->findChildren<QLineEdit*>();

        for (QLineEdit *lineEdit : lineEdits) {
            lineEdit->setText(systemMap[lineEdit->objectName()]);
        }
    }else{
        qDebug()<<"empty";
    }
}

PIDControl::~PIDControl()
{
    delete ui;
}

void PIDControl::on_pushButton_clicked()
{
    // PID 参数
    float p = ui->Kp->text().toFloat();
    float i = ui->Ki->text().toFloat();
    float d = ui->Kd->text().toFloat();
    float dt = ui->delta_t->text().toFloat();
    float pidDir = ui->pidDir->text().toFloat();

    float maxOutput = ui->max_output->text().toFloat();
    float minOutput = ui->min_output->text().toFloat();

    emit savePID(p,i,d,dt,pidDir,maxOutput,minOutput);

}

