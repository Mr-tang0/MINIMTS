#include "calculator.h"
#include "ui_calculator.h"
#include<qmath.h>
#include<QDebug>

calculator::calculator(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::calculator)
{
    ui->setupUi(this);
    this->setWindowFlags(this->windowFlags() &~ Qt::WindowMaximizeButtonHint|Qt::WindowStaysOnTopHint);//禁止最大化

    connect(ui->buttonGroup_number, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),this,
          [=](QAbstractButton *button)
    {
        if(displayNUM=="0")
        {
            ui->lcdNumber->display(button->text());
            displayNUM = button->text();
        }else if(displayNUM=="-0"){
            displayNUM = "-";
            ui->lcdNumber->display(displayNUM+button->text());
            displayNUM = displayNUM+button->text();
        }else
        {
            ui->lcdNumber->display(displayNUM+button->text());
            displayNUM = displayNUM+button->text();
        }

    });

    connect(ui->buttonGroup_sign, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),this,
          [=](QAbstractButton *button)
    {

        num1 = ui->lcdNumber->value();
        displayNUM="0";
        ui->lcdNumber->display(displayNUM);

        sign = button->text();

    });

    ui->lcdNumber->display(displayNUM);

}

calculator::~calculator()
{
    delete ui;
}


void calculator::on_Button_13_clicked()
{
    qDebug()<<"="<<sign<<num1<<num2;
    if(sign==""){
        ui->lcdNumber->display(num1);
        return;
    }

    num2 = ui->lcdNumber->value();
    if(sign=="+"){
        displayNUM = QString::number(num1+num2);
        ui->lcdNumber->display(displayNUM);
    }else if(sign=="-"){
        displayNUM = QString::number(num1-num2);
        ui->lcdNumber->display(num1-num2);
    }else if(sign=="×"){
        displayNUM = QString::number(num1*num2);
        ui->lcdNumber->display(num1*num2);
    }else if(sign=="÷"){
        displayNUM = QString::number(num1/num2);
        ui->lcdNumber->display(num1/num2);
    }
    sign="";
}

void calculator::on_Button_DEL_clicked()
{
    displayNUM = displayNUM.mid(0,displayNUM.length()-1);
    ui->lcdNumber->display(displayNUM);

}

void calculator::on_Button_AC_clicked()
{
    displayNUM = "0";
    ui->lcdNumber->display(displayNUM);
    num1=0;
    num2=0;
    sign = "";
}

void calculator::on_Button_point_clicked()
{
    QString point = ".";
    ui->lcdNumber->display(displayNUM+point);
    displayNUM = displayNUM+point;
}

void calculator::on_Button_minus_clicked()
{
    QString minus = "-";
    ui->lcdNumber->display(minus+displayNUM);
    displayNUM = minus+displayNUM;
}
