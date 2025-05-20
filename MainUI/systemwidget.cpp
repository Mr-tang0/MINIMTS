#include "systemwidget.h"
#include "ui_systemwidget.h"
#include "quihelper.h"
#include "iconhelper.h"
#include "mainwindow.h"
#include "xmlreader.h"


QMap<QString, QMap<QString, QString>> systemWidget::systemMap;

systemWidget::systemWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::systemWidget)
{
    ui->setupUi(this);

    rootPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +"/Tang/MiniMTS/system.xml";

    initSystem();

    QList<int> iconsMain;
    QList<int> iconsConfig;
    QList<QAbstractButton *> btnsConfig;

    iconsMain << 0xf013 << 0xf076;

    btnsMain << ui->normalSet << ui->advanceSetup;
    int count = btnsMain.count();
    for (int i = 0; i < count; ++i) {
        QToolButton *btn = (QToolButton *)btnsMain.at(i);
        btn->setCheckable(true);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        connect(btn, SIGNAL(clicked(bool)), this, SLOT(leftMainClick()));
    }

    IconHelper::StyleColor styleColor;
    styleColor.position = "left";
    styleColor.iconSize = 18;
    styleColor.iconWidth = 35;
    styleColor.iconHeight = 25;
    styleColor.borderWidth = 4;
    styleColor.borderColor = "#242424";
    styleColor.setColor("#484848", "#DCDCDC", "#444444", "#DCDCDC");
    IconHelper::setStyle(ui->menuWidget, btnsMain, iconsMain, styleColor);

    ui->normalSet->click();


    connect(ui->save,&QPushButton::clicked,this,&systemWidget::normolSaveSystem);
    connect(ui->advance_check,&QPushButton::clicked,this,&systemWidget::advanceSaveSystem);

    connect(ui->cancel,&QPushButton::clicked,[=](){emit saved(false);});
    connect(ui->advance_cancel,&QPushButton::clicked,[=](){emit saved(false);});


}

systemWidget::~systemWidget()
{
    delete ui;
}

void systemWidget::initSystem(){
    systemMap = XMLReader::Read_XML_File(rootPath);

    QWidget *widget = ui->stackedWidget->widget(0);
    QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();

    for (QLineEdit *lineEdit : lineEdits) {
        lineEdit->setText(systemMap["normal"][lineEdit->objectName()]);
    }

    widget = ui->stackedWidget->widget(1);
    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        comboBox->setCurrentText(systemMap["advance"][comboBox->objectName()]);
    }

    widget = ui->stackedWidget->widget(2);
    lineEdits.clear();
    lineEdits = widget->findChildren<QLineEdit*>();
    for (QLineEdit *lineEdit : lineEdits) {
        lineEdit->setText(systemMap["users"][lineEdit->objectName()]);
    }


}

void systemWidget::normolSaveSystem(){
    QWidget *widget = ui->stackedWidget->widget(0);
    QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();

    for (QLineEdit *lineEdit : lineEdits) {
        systemMap["normal"][lineEdit->objectName()] = lineEdit->text();
    }

    emit saved(XMLReader::Save_XML_File(systemMap,rootPath));
}

void systemWidget::advanceSaveSystem(){
    QWidget *widget = ui->stackedWidget->widget(1);
    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        systemMap["advance"][comboBox->objectName()] = comboBox->currentText();
    }

    emit saved(XMLReader::Save_XML_File(systemMap,rootPath));
}

void systemWidget::usersSaveSystem(){
    QWidget *widget = ui->stackedWidget->widget(2);
    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        systemMap["users"][comboBox->objectName()] = comboBox->currentText();
    }

    emit saved(XMLReader::Save_XML_File(systemMap,rootPath));
}

void systemWidget::leftMainClick()
{
    QToolButton *b = (QToolButton *)sender();
    QString name = b->text();

    int count = btnsMain.count();
    for (int i = 0; i < count; ++i) {
        QToolButton *btn = btnsMain.at(i);
        btn->setChecked(btn == b);
    }
    if(name == "基本设置"){
        ui->stackedWidget->setCurrentIndex(0);
    }else if (name == "高级设置"){
        ui->stackedWidget->setCurrentIndex(1);
    }else if (name == "用户设置"){
        ui->stackedWidget->setCurrentIndex(2);
    }
}
