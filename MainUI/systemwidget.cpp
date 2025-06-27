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

    iconsMain << 0xf013 << 0xf076<< 0xf033 << 0xf096;

    btnsMain << ui->normalSet << ui->advanceSetup<<ui->cameraSetup<<ui->DICSetup;
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


void systemWidget::dicSaveSystem(){
    QWidget *widget = ui->stackedWidget->widget(2);

    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        systemMap["dic"][comboBox->objectName()] = comboBox->currentText();
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
    }else if (name == "DIC设置"){
        ui->stackedWidget->setCurrentIndex(2);
    }else if(name == "相机设置"){
        emit setupCamera();
    }
}

void systemWidget::on_readSet_clicked()
{
    // 弹出文件选择对话框
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("选择配置文件"),
        QDir::homePath(),             // 默认打开用户主目录
        tr("XML 文件 (*.xml);;所有文件 (*)")
        );

    // 如果用户取消选择，path 为空，直接返回
    if (path.isEmpty()) {
        return;
    }


    QMap<QString,QMap<QString,QString>> tempMap = XMLReader::Read_XML_File(path);

    if (tempMap.isEmpty()) {
        QMessageBox::warning(this, tr("读取失败"), tr("无法读取 XML 文件或文件格式不正确。\n请检查文件内容后重试。"));
        return;
    }

    QWidget *widget = ui->stackedWidget->widget(0);
    QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();

    for (QLineEdit *lineEdit : lineEdits) {
        lineEdit->setText(tempMap["normal"][lineEdit->objectName()]);
    }

    widget = ui->stackedWidget->widget(1);
    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        comboBox->setCurrentText(tempMap["advance"][comboBox->objectName()]);
    }

    widget = ui->stackedWidget->widget(2);
    lineEdits.clear();
    lineEdits = widget->findChildren<QLineEdit*>();
    for (QLineEdit *lineEdit : lineEdits) {
        lineEdit->setText(tempMap["users"][lineEdit->objectName()]);
    }

    QMessageBox::information(this, tr("读取成功"), tr("已经读取配置如下，点击保存立即生效！"));
}


void systemWidget::on_saveSetAs_clicked()
{
    // 弹出文件选择对话框
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("编辑配置文件"),
        QDir::homePath(),             // 默认打开用户主目录
        tr("XML 文件 (*.xml);;所有文件 (*)")
    );

    // 如果用户取消选择，path 为空，直接返回
    if (path.isEmpty()) {
        return;
    }


    QMap<QString,QMap<QString,QString>> tempMap;


    QWidget *widget = ui->stackedWidget->widget(0);
    QList<QLineEdit*> lineEdits = widget->findChildren<QLineEdit*>();

    for (QLineEdit *lineEdit : lineEdits) {
        tempMap["normal"][lineEdit->objectName()] = lineEdit->text();
    }

    widget = ui->stackedWidget->widget(1);
    QList<QComboBox*> comboBoxList = widget->findChildren<QComboBox*>();
    for (QComboBox *comboBox : comboBoxList) {
        tempMap["advance"][comboBox->objectName()] = comboBox->currentText();
    }

    XMLReader::Save_XML_File(tempMap,path);

    QMessageBox::information(this, tr("保存成功"), tr("已经保存配置到指定位置！"));
}


