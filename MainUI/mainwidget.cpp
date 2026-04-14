#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "mainwindow.h"
#include "xmlreader.h"
#include <QtConcurrent>
#include <QDir>

QMap<QString, QString> mainWidget::historyMap;

mainWidget::mainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::mainWidget)
{
    ui->setupUi(this);

    initWidget();

    initChart();

    initConnect();

    initPort();

    // initConnect();

    initSystem();



}

mainWidget::~mainWidget()
{
    delete ui;
}


void mainWidget::initWidget(){
    QList<int> iconsMain;

    iconsMain << 0xf0c1 << 0xf016 <<0xf16a<<0xf0c7<<0xf114;
    btnsMain << ui->btn_connect << ui->btn_pro <<ui->btn_test <<ui->btn_save<<ui->btn_read;
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
    IconHelper::setStyle(ui->widget_menu, btnsMain, iconsMain, styleColor);

    ui->btn_emergence->setFixedSize(QSize(120,80));
    ui->btn_start->setFixedSize(QSize(120,80));
    ui->btn_stop->setFixedSize(QSize(120,80));

    ui->speedCW->setFixedSize(QSize(120,60));
    ui->speedCCW->setFixedSize(QSize(120,60));
    ui->speedStop->setFixedSize(QSize(120,60));
}

void mainWidget::initChart(){
    ui->graphics->addSeries("载荷-时间", "Force", "Time");
    ui->graphics->addSeries("位移-时间", "Length", "Time");
    ui->graphics->addSeries("视频引伸-时间", "视频引伸", "Time");
    ui->graphics->addSeries("应力-应变", "Stress", "Strian");
    ui->graphics->addSeries("应力-视频应变", "Stress", "E-Strian");
    ui->graphics->addSeries("速度-时间", "Speed", "Time");
    ui->graphics->addSeries("AX-时间", "AX", "Time");
    ui->graphics->addSeries("AY-时间", "AY", "Time");
    ui->graphics->addSeries("BX-时间", "BX", "Time");
    ui->graphics->addSeries("BY-时间", "BY", "Time");

    ui->graphics->longShow();
    ui->graphics->showSeries("载荷-时间");

    ui->ForceLCD->setKey("载荷 N");
    ui->ForceLCD->setDecimals(1);

    ui->lengthLCD->setKey("位移 mm");
    ui->lengthLCD->setDecimals(3);

    ui->stressLCD->setKey("应力 MPa");
    ui->stressLCD->setDecimals(1);

    ui->strainLCD->setKey("应变");
    ui->strainLCD->setDecimals(3);

    ui->DICLCD->setKey("视频引伸 mm");
    ui->DICLCD->setDecimals(3);

    ui->eStrainLCD->setKey("视频应变");
    ui->eStrainLCD->setDecimals(3);

    ui->timeLCD->setKey("时长 Secs");
    ui->timeLCD->setDecimals(1);

    ui->graphics->hideSeries("AX-时间");
    ui->graphics->hideSeries("AY-时间");
    ui->graphics->hideSeries("BX-时间");
    ui->graphics->hideSeries("BY-时间");
    ui->graphics->hideSeries("视频引伸-时间");
    ui->graphics->hideSeries("应力-视频应变");

    ui->DICLCD->hide();
    ui->eStrainLCD->hide();

}

void mainWidget::initPort(){
    ui->portName->clear();
    ui->portName->addItems(minilogic->findPort());

    ui->buandRate->clear();
    QStringList buandRateList = {"115200"};
    ui->buandRate->addItems(buandRateList);

    ui->refresh_hik->click();
}

void mainWidget::initConnect(){

    //*****************************************连接窗口****************************************************
    connect(dataFreshTimer,&QTimer::timeout,this,&mainWidget::freshData);

    // 串口刷新按钮
    connect(ui->refresh,&QPushButton::clicked,this,[=](){initPort();});

    //  串口连接按钮
    connect(ui->connect,&QPushButton::clicked,this,[=](){
        if(ui->connect->text()=="连接"){
            QString portName = ui->portName->currentText();
            int buandRate = ui->buandRate->currentText().toInt();
            motorConnectFlag = minilogic->connectDevice(portName,buandRate, systemWidget::systemMap["normal"]["sampleRate"].toInt());
            if(motorConnectFlag){

                ui->connect->setText("断开");
                zreoTime = QDateTime::currentMSecsSinceEpoch();
                addStateBox("连接到串口:"+portName);
                addStateBox("波特率:"+QString::number(buandRate));

            }else addStateBox("连接失败:"+portName);
            emit connected(motorConnectFlag);

        }else{
            minilogic->disConnectDevice();
            ui->connect->setText("连接");
            motorConnectFlag = false;
            addStateBox("断开连接");
            emit disconnected();
        }
    });

    // 串口测试按钮
    connect(ui->testConnect,&QPushButton::clicked,[=](){
        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }
        emit message("测试连接中");
        addStateBox("->测试连接中");

        minilogic->testConnect();
        Tool::delay(1000);

    });

    // 快捷打开设置按钮
    connect(ui->toSet,&QPushButton::clicked,[=](){emit btnClicked("toSet");});

    // 清空状态框按钮
    connect(ui->clearWin,&QPushButton::clicked,[=](){
        clearStateBox();
        addStateBox("已清除");
        addStateBox("----------------");
    });

    connect(minilogic,&MiniMTSLogic::testOK,[=](QString device){addStateBox(device+"测试在线");});

    connect(ui->refresh_hik,&QPushButton::clicked,this,[=](){
        devices.clear();
        ui->cameraList->clear();
        hikWin.camera->EnumDevices(devices, hikWin.camera);
        ui->cameraList->addItems(devices.keys());
    });

    connect(ui->connect_hik,&QPushButton::clicked,this,[=](){
        if(ui->connect_hik->text()=="连接")
        {
            QString device = ui->cameraList->currentText();
            int ret = hikWin.camera->Open(device);
            if(ret ==MV_OK){
                hikWin.camera->RegisterImageCallback();

                hikWin.camera->SetTriggerMode(MV_TRIGGER_MODE_OFF);
                hikWin.camera->StartGrabbing();

                ui->connect_hik->setText("断开");

                hikWin.resize(800,600);
                hikWin.show();
            }

        }else{
            hikWin.camera->StopGrabbing();
            hikWin.camera->Close();

            ui->connect_hik->setText("连接");
            hikWin.close();
        }
    });

    //************************************设备运动窗口**********************************************

    // 按钮按下，cw运动
    connect(ui->speedCW,&QPushButton::pressed,[=](){
        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }

        float speed = ui->speed->value();
        minilogic->motorSpeedMove(speed);
        moveFlag = true;
        addState("正向运动中");
    });

    // 按钮按下，ccw运动
    connect(ui->speedCCW,&QPushButton::pressed,[=](){
        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }

        float speed = ui->speed->value();
        minilogic->motorSpeedMove(-speed);
        addState("负向运动中");
        moveFlag = true;
    });

    // 按钮松开，停止运动
    connect(ui->speedCW,&QPushButton::released,[=](){
        minilogic->motorStop();
        minilogic->motorStop();
        addState("运动停止");
        moveFlag = false;
    });


    // 按钮松开，停止运动
    connect(ui->speedCCW,&QPushButton::released,[=](){
        minilogic->motorStop();
        minilogic->motorStop();
        addState("运动停止");
        moveFlag = false;
    });

    // 停止运动
    connect(ui->speedStop,&QPushButton::clicked,[=](){
        minilogic->motorStop();
        minilogic->motorStop();
        addState("运动停止");
        moveFlag = false;
    });

    connect(ui->ForceLCD, &MyLCD::doubleClicked, [=]() {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "确认", "您确定要将载荷归零吗？",
                                                                  QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) { // 如果用户点击了“是”
            minilogic->setChannalZero("weigh");
            emit message("载荷归零");
            addState("载荷归零");
        } else { // 如果用户点击了“否”
            addState("用户取消");
        }
    });

    connect(ui->lengthLCD, &MyLCD::doubleClicked, [=]() {
        // 弹出确认对话框
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "您确定要将位移归零吗？",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            minilogic->setChannalZero("motor");
            emit message("位移归零");
            addState("位移归零");
        }else { // 如果用户点击了“否”
            addState("用户取消");
        }
    });

    connect(ui->DICLCD, &MyLCD::doubleClicked, [=]() {
        // 弹出确认对话框
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "您确定要将光学引伸距离归零吗？",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            hikWin.camera->setCurrentZreo();
            emit message("光学引伸距离归零");
            addState("光学引伸距离归零");
        }else { // 如果用户点击了“否”
            addState("用户取消");
        }
    });

    connect(ui->timeLCD, &MyLCD::doubleClicked, [=]() {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "您确定要将时间归零吗？",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            zreoTime = QDateTime::currentMSecsSinceEpoch();
            emit message("时间归零");
            addState("时间归零");
        }else { // 如果用户点击了“否”
            addState("用户取消");
        }
    });

    connect(ui->clearChart,&QPushButton::clicked,[=](){
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "是否要清除所有数据？",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            zreoTime = QDateTime::currentMSecsSinceEpoch();
            ui->graphics->clearAll();
            emit message("已清除");
            addState("数据已清除");
        }

    });

    connect(ui->setZreo,&QPushButton::clicked,[=](){
        // 弹出确认对话框
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "您确定要将位移归零吗？",
            QMessageBox::Yes | QMessageBox::No
            );
        if(reply==QMessageBox::Yes){
            minilogic->setChannalZero("motor");
            hikWin.camera->setCurrentZreo();
        }else{
            emit message("用户取消");
        }

    });



    //************************************项目设置窗口**********************************************

    connect(ui->cancel,&QPushButton::clicked, this, [=](){
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "当前项目未保存，是否取消？",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            ui->btn_test->click();
        }
    });

    connect(ui->stopAction,&QComboBox::currentTextChanged,this,[=](QString text){
        if(text.contains("("))
            ui->stopActionValue->show();
        else
            ui->stopActionValue->hide();
    });
    ui->stopActionValue->hide();


    connect(ui->chooseFilePath_pushButton, &QPushButton::clicked, this, [=](){
        QString home = MainWindow::user->localPath;
        QString filePath = QFileDialog::getExistingDirectory(this, tr("Select Directory"), home);
        if (!filePath.isEmpty()) {
            ui->filePath->setText(filePath);
        }
    });

    connect(ui->chooseDICFilePath, &QPushButton::clicked, this, [=](){
        QString home = MainWindow::user->localPath;
        QString filePath = QFileDialog::getExistingDirectory(this, tr("Select DIC Directory"), home);
        if (!filePath.isEmpty()) {
            ui->DICFilePath->setText(filePath);
        }
    });

    connect(ui->selectPointA,&QPushButton::clicked,this,[=](){
        hikWin.camera->addTracker("A");
    });
    connect(ui->selectPointB,&QPushButton::clicked,this,[=](){
        hikWin.camera->addTracker("B");
    });

    connect(ui->autoSelectPositionLine,&QPushButton::clicked,this,[=](){
        hikWin.camera->autoSelectPositionLine("Draw the area where the gauge segment is located");
    });

    connect(ui->selectPositionLine,&QPushButton::clicked,this,[=](){
        hikWin.camera->selectPositionLine("Draw the direction of the gauge segment");
    });


    connect(hikWin.camera, &IndustryCamara::ROI, this, [=](QString ID, cv::Rect rect){
        QString rectText = QString("X:%1 Y:%2 W:%3 H:%4").arg(rect.x).arg(rect.y).arg(rect.width).arg(rect.height);
        if(ID=="A"){
            ui->pointA->setText(rectText);
        }else if(ID=="B"){

            ui->pointB->setText(rectText);
        }

    });

    //保存项目
    connect(ui->Checked,&QPushButton::clicked,[=](){saveProject();});

    connect(ui->drawPix,&QPushButton::clicked,this,[=](){hikWin.camera->selectLine("Draw a line you can measure");});

    connect(hikWin.camera,&IndustryCamara::LINE,this,[=](float len){ui->pix->setText(QString::number(len));});



}





void mainWidget::limitStopOption(bool flag, int dir){

    if (flag){
        minilogic->motorStop();
        minilogic->motorStop();
    }

    ui->btn_start->setEnabled(!flag);
    if (dir == systemWidget::systemMap["normal"]["cwLimit"].toInt())
        ui->speedCW->setEnabled(!flag);
    else if (dir == systemWidget::systemMap["normal"]["ccwLimit"].toInt())
        ui->speedCCW->setEnabled(!flag);
    else
    {
        ui->speedCW->setEnabled(!flag);
        ui->speedCCW->setEnabled(!flag);
    }
}


void mainWidget::leftMainClick()
{
    QToolButton *b = (QToolButton *)sender();
    QString name = b->text();

    int count = btnsMain.count();
    for (int i = 0; i < count; ++i) {
        QToolButton *btn = btnsMain.at(i);
        btn->setChecked(btn == b);
    }
    name = name.remove(" ");
    if(name == "连接"){
        ui->stackedWidget->setCurrentIndex(0);
        initSystem();
    }else if (name == "项目"){
        ui->stackedWidget->setCurrentIndex(1);
        initSystem();
    }else if (name == "测试"){
        ui->stackedWidget->setCurrentIndex(2);
    }else if (name == "保存"){
        QMap<QString,QLineSeries*> smap;
        ui->graphics->getAllSeries(&smap);

        if(smap.first()->count()<=10){
            emit message("数据量过小无法保存");
            return;
        }else emit message("保存文件中");


        QFutureWatcher<bool> *futureWatcher = new QFutureWatcher<bool>(this);
        connect(futureWatcher, &QFutureWatcher<bool>::finished, this, [=]() {
            bool result = futureWatcher->result();
            // 更新UI或者其他操作
            if(result)emit message("文件保存成功");
            else emit message("文件保存失败");
        });

        QFuture<bool> future = QtConcurrent::run(this, &mainWidget::saveCsv, smap);
        futureWatcher->setFuture(future);

    }else if (name == "读取"){
        if(motorConnectFlag){
            emit message("连接未断开");
            return;
        }else{
            QString filePath = QFileDialog::getOpenFileName(this, "选择文件", historyMap["userfilePath"], "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)");

            if(filePath=="")
                return;
            else
                ui->stackedWidget->setCurrentIndex(2);


            QFutureWatcher<bool> *futureWatcher = new QFutureWatcher<bool>(this);
            connect(futureWatcher, &QFutureWatcher<bool>::finished, this, [=]() {
                bool result = futureWatcher->result();
                // 更新UI或者其他操作
                if(result)emit message("文件读取成功");
                else emit message("文件读取失败");
            });

            QFuture<bool> future = QtConcurrent::run(this, &mainWidget::readCsv, filePath);
            futureWatcher->setFuture(future);
        }
    }
}

bool mainWidget::readCsv(QString path){
    QStringList header; //("force", "length", "strain", "stress", "speed", "time")
    QList<QList<float>> data = Tool::loadCsvData(path,header);

    if(data.length()<=0||data[0].length()<=0)return false;

    QList<QPointF> forceTimePoints;
    QList<QPointF> lengthTimePoints;
    QList<QPointF> stresStrainPoints;
    QList<QPointF> speedTimePoints;

    for (int var = 0; var < data[0].length(); ++var) {
        forceTimePoints.append(QPointF(data[5][var], data[0][var]));
        lengthTimePoints.append(QPointF(data[5][var], data[1][var]));
        stresStrainPoints.append(QPointF(data[2][var], data[3][var]));
        speedTimePoints.append(QPointF(data[5][var], data[4][var]));
    }

    ui->graphics->clearAll();
    ui->graphics->addPoints("载荷-时间",forceTimePoints);
    ui->graphics->addPoints("位移-时间",lengthTimePoints);
    ui->graphics->addPoints("应力-应变",stresStrainPoints);
    ui->graphics->addPoints("速度-时间",speedTimePoints);

    return true;
}

bool mainWidget::saveCsv(QMap<QString, QLineSeries*> data){
    if(data.first()->count()<=0)return false;


    QMap<QString, QList<float>> dataMap;

    for (auto it = data.begin(); it != data.end(); ++it) {
        QString seriesName = it.key();
        QLineSeries *series = it.value();

        QStringList titles=seriesName.split("-");

        const auto points = series->points();

        QList<float> xData,yData;
        foreach (const QPointF &point, points) {
            xData.append(point.x());
            yData.append(point.y());
        }

        if(!dataMap.contains(titles[0])){
            dataMap[titles[0]] = yData;
        }
        if(!dataMap.contains(titles[1])){
            dataMap[titles[1]] = xData;
        }

    }

    QStringList header = {"force","strain(motor)","strain(video)","stress", "length(motor)","length(video)","speed", "AX", "AY", "BX", "BY", "time"};

    QList<QList<float>> newdatalist;

    newdatalist.append(dataMap["载荷"]); //force

    newdatalist.append(dataMap["应变"]); //strain
    newdatalist.append(dataMap["视频应变"]); //e-strian

    newdatalist.append(dataMap["应力"]); //stress

    newdatalist.append(dataMap["位移"]);//length
    newdatalist.append(dataMap["视频引伸"]); //DIC

    newdatalist.append(dataMap["速度"]); //speed

    newdatalist.append(dataMap["AX"]); //AX
    newdatalist.append(dataMap["AY"]); //AY
    newdatalist.append(dataMap["BX"]); //BX
    newdatalist.append(dataMap["BY"]); //BY

    newdatalist.append(dataMap["时间"]); //time


    return saveCsv(newdatalist,header);
}

bool mainWidget::saveCsv(QList<QList<float>> data,QList<QString> header){
    if(data.empty()||data[0].empty())return false;

    QString fileName = historyMap["userDefaultName"]+systemWidget::systemMap["advance"]["format"];

    QString filePath = historyMap["userfilePath"]+"/"+QDateTime::currentDateTime().toString("yy-MM-dd");

    if(systemWidget::systemMap["advance"]["enableFilter"]=="yes"){
        //滤波开启
        int window = systemWidget::systemMap["advance"]["filterWindow"].toInt();
        QString type = systemWidget::systemMap["advance"]["filterMethod"];
        for (int i = 0;i<data.length();i++) {
            Tool::dataSmooth(data[i],type,window);
        }

    }
    if(systemWidget::systemMap["advance"]["enableInterpolation"]=="yes"){
        //插值开启

        int count = systemWidget::systemMap["advance"]["interpolationCount"].toInt();
        QString type = systemWidget::systemMap["advance"]["interpolationMethod"];
        for (int i = 0;i<data.length();i++) {
            Tool::interpolation(data[i],type,count);
        }
    }

    QMap<QString,QString> fileDetails;
    fileDetails["userName"] = testDetails.userName;
    fileDetails["testNumber"] = testDetails.testNumber;
    fileDetails["testTime"] = testDetails.testTime;
    fileDetails["testType"] = testDetails.testType;
    fileDetails["targetLength"] = QString::number(testDetails.length);
    fileDetails["targetSpeed"] = QString::number(testDetails.speed);
    fileDetails["sampleSection"] = QString::number(testDetails.sampleSquare);
    fileDetails["sampleLength"] = QString::number(testDetails.sampleLength);
    fileDetails["autoStopType"] = testDetails.autoStopType;
    fileDetails["autoStopValue"] = QString::number(testDetails.autoStopValue);
    fileDetails["userFilePath"] = testDetails.userFilePath;
    fileDetails["userFileName"] = testDetails.userFileName;

    fileDetails["DICFilePath"] = testDetails.DICFilePath;
    fileDetails["DICFileName"] = testDetails.DICFileName;
    fileDetails["DICTriggr"] = testDetails.DICTriggr;
    fileDetails["DICDuration"] = QString::number(testDetails.DICDuration);
    fileDetails["DICDetectEnable"] = testDetails.DICDetectEnable?"true":"false";

    bool flag = Tool::saveCsvData(data,header,fileDetails,filePath+"/"+fileName);
    return flag;
}

void mainWidget::getQssColor(const QString &qss, const QString &flag, QString &color)
{
    int index = qss.indexOf(flag);
    if (index >= 0) {
        color = qss.mid(index + flag.length(), 7);
    }
}

void mainWidget::getQssColor(const QString &qss, QString &textColor, QString &panelColor,
                             QString &borderColor, QString &normalColorStart, QString &normalColorEnd,
                             QString &darkColorStart, QString &darkColorEnd, QString &highColor)
{
    getQssColor(qss, "TextColor:", textColor);
    getQssColor(qss, "PanelColor:", panelColor);
    getQssColor(qss, "BorderColor:", borderColor);
    getQssColor(qss, "NormalColorStart:", normalColorStart);
    getQssColor(qss, "NormalColorEnd:", normalColorEnd);
    getQssColor(qss, "DarkColorStart:", darkColorStart);
    getQssColor(qss, "DarkColorEnd:", darkColorEnd);
    getQssColor(qss, "HighColor:", highColor);
}




void mainWidget::on_btn_start_clicked()
{
    if(!motorConnectFlag){
        emit message("串口未连接");
        return;
    }

    minilogic->run(false);
    ui->btn_stop->setText("停止");


    if(testDetails.length==0||testDetails.speed==0||testDetails.sampleSquare==0||testDetails.sampleLength==0)
    {
        emit message("项目参数保存有误，请检查");
        return;
    }

    if(abs(ui->lengthLCD->currentValue())>=0.001)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "当前位移并非零点，是否继续测试？",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {

        }else{
            emit message("测试取消");
            addState("测试取消");
            return;
        }
    }

    if(abs(testDetails.sampleLength/testDetails.speed)>1000||abs(testDetails.sampleLength/testDetails.speed)<100)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            QString("当前标距段设置运动速度不匹配, 标距段长度：%1 mm, 速度设置：%2 mm/s, 是否继续？").arg(testDetails.sampleLength).arg(testDetails.speed),
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {

        }else{
            emit message("测试取消");
            addState("测试取消");
            return;
        }
    }

    ui->clearChart->click();

    if(ui->btn_start->text()=="开始"){

        if(testDetails.testType=="strech"){
            minilogic->motorLengthMove(testDetails.length,testDetails.speed);
            displayDir=1;
        }else if(testDetails.testType=="compress"){
            minilogic->motorLengthMove(-testDetails.length,testDetails.speed);
            displayDir=-1;
        }
    }

    if(testDetails.DICDetectEnable&&hikWin.camera->isOpened()&&testDetails.DICDuration>0)
    {
        QString fullPath = testDetails.DICFilePath + "/" + testDetails.DICFileName;
        if(testDetails.DICFilePath.right(1)=="/")fullPath = testDetails.DICFilePath + testDetails.DICFileName;

        hikWin.camera->startDIC(testDetails.DICFileName, fullPath, testDetails.DICDuration);
        emit message("开始DIC采集");
    }

    if(testDetails.DICTriggrEnable)
    {
        //单次触发
        if(testDetails.DICDuration==0){
            minilogic->triggr();
            emit message("触发信号已发送");
        }else{
            //循环触发
            minilogic->TriggrOn(0,testDetails.DICDuration);
            emit message("开始触发信号");
        }


    }

    emit message("开始测试");
    addState("开始测试");
    moveFlag = true;
}

void mainWidget::on_btn_stop_clicked()
{
    if(testDetails.autoStopType=="Force Hold"){
        if(ui->btn_stop->text()=="停止"){
            if(!motorConnectFlag){
                emit message("串口未连接");
                return;
            }

            minilogic->setTargetForce(minilogic->currentWeighValue);
            minilogic->run(true);
            //数据采集存在一点延时，补偿回来
            QTimer::singleShot(260,this,[=](){minilogic->setTargetForce(minilogic->currentWeighValue);});


            ui->btn_stop->setText("停止保持");
            emit message("保持当前力");
            addState("测试停止");
        }else if(ui->btn_stop->text()=="停止保持"){
            if(!motorConnectFlag){
                emit message("串口未连接");
                return;
            }

            ui->btn_stop->setText("停止");
            minilogic->run(false);

            moveFlag = false;
            emit message("测试停止");
            addState("测试停止");
        }
    }else{
        if(hikWin.camera->isOpened())
            hikWin.camera->stopDIC();

        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }

        minilogic->motorStop();

        moveFlag = false;
        minilogic->run(false);

        emit message("测试停止");
        addState("测试停止");
    }

}

void mainWidget::addStateBox(QString message){
    ui->textBrowser->append(message);
    addState(message);
}

void mainWidget::addState(QString msg){
    ui->state->setText(msg);
}

void mainWidget::clearStateBox(){
    ui->textBrowser->clear();

}

void mainWidget::on_btn_emergence_clicked()
{
    for(int i = 0;i<10;i++){
        minilogic->motorStop();
        minilogic->motorOff();
    }
    emit message("急停被按下！");
    emit errOccured("emergence");
}
