#include "systemwidget.h"
#include "ui_systemwidget.h"
#include "quihelper.h"
#include "iconhelper.h"
#include "mainwindow.h"
#include "tool.h"

systemWidget::systemWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::systemWidget)
{
    ui->setupUi(this);

    initSystem();




    QList<int> iconsMain;
    QList<int> iconsConfig;
    QList<QAbstractButton *> btnsConfig;

    iconsMain << 0xf013 << 0xf076 <<0xf007;

    btnsMain << ui->normalSet << ui->advanceSetup <<ui->userSet;
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

    connect(ui->save,&QPushButton::clicked,this,[=](){
        normolSaveSystem();
    });
    connect(ui->advance_check,&QPushButton::clicked,this,[=](){
        advanceSaveSystem();
    });

    connect(ui->cancel,&QPushButton::clicked,[=](){emit saved(false);});
    connect(ui->advance_cancel,&QPushButton::clicked,[=](){emit saved(false);});


}

systemWidget::~systemWidget()
{
    delete ui;
}

void systemWidget::initSystem(){
    systemMap = MainWindow::systemMap;

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

//    widget = ui->stackedWidget->widget(1);
//    lineEdits.clear();
//    lineEdits = widget->findChildren<QLineEdit*>();
//    for (QLineEdit *lineEdit : lineEdits) {
//        systemMap["advance"][lineEdit->objectName()] = lineEdit->text();
//    }

//    widget = ui->stackedWidget->widget(2);
//    lineEdits.clear();
//    lineEdits = widget->findChildren<QLineEdit*>();
//    for (QLineEdit *lineEdit : lineEdits) {
//        systemMap["users"][lineEdit->objectName()] = lineEdit->text();
//    }

    MainWindow::systemMap = systemMap;
    Tool::writeXMlFile(MainWindow::systemMap["self"]["path"],MainWindow::systemMap);

    emit saved(true);
}

void systemWidget::advanceSaveSystem(){
    QWidget *widget = ui->stackedWidget->widget(1);
    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        systemMap["advance"][comboBox->objectName()] = comboBox->currentText();
    }
    MainWindow::systemMap = systemMap;
    Tool::writeXMlFile(MainWindow::systemMap["self"]["path"],MainWindow::systemMap);

    emit saved(true);
}

void systemWidget::usersSaveSystem(){
    QWidget *widget = ui->stackedWidget->widget(2);
    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        systemMap["users"][comboBox->objectName()] = comboBox->currentText();
    }
    MainWindow::systemMap = systemMap;
    Tool::writeXMlFile(MainWindow::systemMap["self"]["path"],MainWindow::systemMap);

    emit saved(true);
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
