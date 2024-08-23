#include "mylcd.h"
#include "ui_mylcd.h"

MyLCD::MyLCD(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MyLCD)
{
    ui->setupUi(this);
}

MyLCD::~MyLCD()
{
    delete ui;
}
void MyLCD::setBackground(){

}
void MyLCD::setKey(QString key){
    ui->key->setText(key);
}
void MyLCD::display(float value){
    QString displayNum = QString::number(value, 'f', decimals);
    ui->value->setText(displayNum);
}

void MyLCD::setFont(int font){

}

void MyLCD::setDecimals(int decimal){
    decimals = decimal;
    display(ui->value->text().toFloat());
}

float MyLCD::currentValue(){
    return ui->value->text().toFloat();
}
QString MyLCD::currentKey(){
    return ui->key->text();
}
