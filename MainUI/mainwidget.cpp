#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "mainwindow.h"
#include "xmlreader.h"
#include <QtConcurrent>

mainWidget::mainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::mainWidget)
{
    ui->setupUi(this);

    initWidget();

    initChart();

    initPort();

    initConnect();

    initSystem();

    ui->btn_emergence->setFixedSize(QSize(120,80));
    ui->btn_start->setFixedSize(QSize(120,80));
    ui->btn_stop->setFixedSize(QSize(120,80));

    ui->speedCW->setFixedSize(QSize(120,60));
    ui->speedCCW->setFixedSize(QSize(120,60));
    ui->speedStop->setFixedSize(QSize(120,60));

}

mainWidget::~mainWidget()
{
    minilogic->motorStop();
    minilogic->motorStop();
    minilogic->motorOff();
    minilogic->motorOff();
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
    ui->btn_connect->click();
}

void mainWidget::initChart(){
    ui->graphics->addSeries("载荷-时间", "Force", "Time");
    ui->graphics->addSeries("位移-时间", "Length", "Time");
    ui->graphics->addSeries("应力-应变", "Stress", "Strian");
    ui->graphics->addSeries("速度-时间", "Speed", "Time");

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

    ui->speedLCD->setKey("速度 mm/s");
    ui->speedLCD->setDecimals(3);

    ui->timeLCD->setKey("时长 Secs");
    ui->timeLCD->setDecimals(1);
}

void mainWidget::initPort(){
    ui->portName->clear();
    ui->portName->addItems(minilogic->findPort());

    ui->buandRate->clear();
    QStringList buandRateList = {"9600","19200","38400","57600","115200","230400","460800"};
    ui->buandRate->addItems(buandRateList);

}

void mainWidget::initConnect(){

    connect(dataFreshTimer,&QTimer::timeout,this,&mainWidget::freshData);

    // 串口刷新按钮
    connect(ui->refresh,&QPushButton::clicked,this,[=](){
        initPort();
        emit refresh();
    });

    //  串口连接按钮
    connect(ui->connect,&QPushButton::clicked,this,[=](){
        if(ui->connect->text()=="连接"){
            QString portName = ui->portName->currentText();
            int buandRate = ui->buandRate->currentText().toInt();
            motorConnectFlag = minilogic->connectDevice(portName,buandRate, 50);
            if(motorConnectFlag){

                ui->connect->setText("断开");
                addStateBox("连接到串口:"+portName);
                addStateBox("波特率:"+QString::number(buandRate));
                zreoTime = QDateTime::currentMSecsSinceEpoch();

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
    connect(ui->toSet,&QPushButton::clicked,[=](){
        emit btnClicked("toSet");
    });

    // 清空状态框按钮
    connect(ui->clearWin,&QPushButton::clicked,[=](){
        clearStateBox();
        addStateBox("已清除");
        addStateBox("----------------");
    });

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


    connect(ui->Checked,&QPushButton::clicked,[=](){
        if(ui->targetLength->value()<=0){
            emit message("目标运动距离不能为零");return;
        }else if(ui->targetSpeed->value()<=0){
            emit message("目标运动速度不能为零");return;
        }else if(ui->size->value()<=0){
            emit message("样品尺寸不能为零");return;
        }else if(ui->length->value()<=0){
            emit message("样品标距段不能为零");return;
        }


        testDetails.userName = ui->userName_lineEidt->text();
        testDetails.testNumber = ui->number_lineEidt->text();
        testDetails.testTime = ui->time_lineEidt->text();

        if(ui->pull_radioButton->isChecked())testDetails.testType = "strech";
        else if(ui->push_radioButton->isChecked())testDetails.testType = "compress";
        testDetails.length = ui->targetLength->value();
        testDetails.speed = ui->targetSpeed->value();

        testDetails.sampleSquare = ui->size->value();
        testDetails.sampleLength = ui->length->value();
        testDetails.autoStopType = ui->stopAction->currentText();
        testDetails.autoStopValue = ui->stopActionValue->value();

        testDetails.userFilePath = ui->filePath->text();
        testDetails.userFileName = ui->fileName->text();

        MainWindow::user.userfilePath = testDetails.userFilePath;
        MainWindow::user.userDefaultName = testDetails.userFileName;

        historyMap[MainWindow::user.name]["type"] = ui->pull_radioButton->isChecked()?"pull_radioButton":"push_radioButton";

        historyMap[MainWindow::user.name]["targetLength"] = QString::number(ui->targetLength->value());
        historyMap[MainWindow::user.name]["targetSpeed"] = QString::number(ui->targetSpeed->value());
        historyMap[MainWindow::user.name]["size"] = QString::number(ui->size->value());
        historyMap[MainWindow::user.name]["length"] = QString::number(ui->length->value());

        historyMap[MainWindow::user.name]["stopAction"] = ui->stopAction->currentText();
        historyMap[MainWindow::user.name]["stopActionValue"] = QString::number(ui->stopActionValue->value());

        historyMap[MainWindow::user.name]["index"] = ui->number_lineEidt->text();

        XMLReader::Save_XML_File(historyMap,userHistroyPath);

        ui->btn_test->click();
        emit message("项目保存成功");
    });

    connect(ui->cancel,&QPushButton::clicked,[=](){
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
        ui->lengthLCD->doubleClick();
        ui->speedLCD->doubleClick();
    });

    connect(ui->chooseFilePath_pushButton, &QPushButton::clicked, this, [=](){
        QString home = MainWindow::user.userfilePath;
        QString filePath = QFileDialog::getExistingDirectory(this, tr("Select Directory"), home);
        if (!filePath.isEmpty()) {
            ui->filePath->setText(filePath);
        }
    });

    connect(minilogic,&MiniMTSLogic::testOK,[=](QString device){addStateBox(device+"测试在线");});

}

void mainWidget::initSystem(){
    //初始化连接界面
    ui->portName->setCurrentText(systemWidget::systemMap["normal"]["defaultPort"]);
    ui->buandRate->setCurrentText(systemWidget::systemMap["normal"]["defaultBuandRate"]);

    // 初始化运动系统
    MINI_MotorDetails md;
    md.ID = systemWidget::systemMap["normal"]["motorID"];
    md.dirFlag = systemWidget::systemMap["normal"]["motorDir"].toInt();
    md.resolution = systemWidget::systemMap["normal"]["motorResulution"].toFloat();

    MINI_WeighDetails wd;
    wd.ID = systemWidget::systemMap["normal"]["forceID"];
    wd.forceRate = systemWidget::systemMap["normal"]["forceResulution"].toFloat();
    wd.forceFlag = systemWidget::systemMap["normal"]["forceDir"].toFloat();

    MINI_LimitDetails ld;
    ld.ID = systemWidget::systemMap["normal"]["hardLimitID"];
    ld.cwLimit = systemWidget::systemMap["normal"]["cwLimit"].toInt();
    ld.ccwLimit = systemWidget::systemMap["normal"]["ccwLimit"].toInt();

    minilogic->setDevicesParams(md,wd,ld);

    //初始化表单界面
    userHistroyPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +"/Tang/MiniMTS/Users/" +MainWindow::user.name;
    userHistroyPath = userHistroyPath +"/histroy.xml";

    //获取特定用户历史文件
    historyMap = XMLReader::Read_XML_File(userHistroyPath);
    // qDebug()<<userHistroyPath;

    //将历史添加到表格
    ui->userName_lineEidt->setText(MainWindow::user.name);
    ui->time_lineEidt->setText(QDateTime::currentDateTime().toString("yy-MM-dd-hh:mm:ss"));

    if(historyMap[MainWindow::user.name]["type"]=="pull_radioButton")
        ui->pull_radioButton->setChecked(true);
    else
        ui->push_radioButton->setChecked(true);

    ui->targetLength->setValue(historyMap[MainWindow::user.name]["targetLength"].toFloat());
    ui->targetSpeed->setValue(historyMap[MainWindow::user.name]["targetSpeed"].toFloat());
    ui->size->setValue(historyMap[MainWindow::user.name]["size"].toFloat());
    ui->length->setValue(historyMap[MainWindow::user.name]["length"].toFloat());
    ui->stopAction->setCurrentText(historyMap[MainWindow::user.name]["stopAction"]);
    ui->stopActionValue->setValue(historyMap[MainWindow::user.name]["stopActionValue"].toFloat());

    ui->number_lineEidt->setText(QString::number(historyMap[MainWindow::user.name]["index"].toInt()+1));

    ui->filePath->setText(MainWindow::user.userfilePath);
    ui->fileName->setText(MainWindow::user.userDefaultName);

    //更新采样率
    dataFreshTimer->stop();
    dataFreshTimer->start(1000/systemWidget::systemMap["normal"]["sampleRate"].toInt());


    //根据设置更改表格显示

    //曲线缩放
    if(systemWidget::systemMap["advance"]["enableZoom"]=="是")
    {
        ui->graphics->axisZoom("x", 0.1, 1);
        ui->graphics->axisZoom("y", 0.1, 1);
    }else{
        ui->graphics->axisZoom("x", 0, 0);
        ui->graphics->axisZoom("y", 0, 0);
    }

    //曲线平滑
    if(systemWidget::systemMap["advance"]["enableFilter_timely"]=="yes")
        ui->graphics->setSmooth(systemWidget::systemMap["advance"]["filterMethod_timely"],systemWidget::systemMap["advance"]["filterWindow_timely"].toInt());
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
            QString filePath = QFileDialog::getOpenFileName(this, "选择文件", MainWindow::user.userfilePath, "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)");

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

    QStringList header;//("force", "length", "strain", "stress", "speed", "time")
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

    QList<QList<float>> datalist;

    for (auto it = data.begin(); it != data.end(); ++it) {
        QString seriesName = it.key();
        QLineSeries *series = it.value();

        const auto points = series->points();

        QList<float> xData,yData;
        foreach (const QPointF &point, points) {
            xData.append(point.x());
            yData.append(point.y());
        }
        datalist.append(xData);
        datalist.append(yData);
    }

    QList<QList<float>> newdatalist;
    //force
    newdatalist.append(datalist[5]);
    newdatalist.append(datalist[1]);
    newdatalist.append(datalist[2]);
    newdatalist.append(datalist[3]);
    newdatalist.append(datalist[7]);
    newdatalist.append(datalist[6]);


    QStringList header = {"force","length","strain","stress", "speed","time"};
    return saveCsv(newdatalist,header);
}

bool mainWidget::saveCsv(QList<QList<float>> data,QList<QString> header){
    //考虑进子线程计算

    if(data.empty()||data[0].empty())return false;

    QString fileName = MainWindow::user.userDefaultName+systemWidget::systemMap["advance"]["format"];

    QString filePath = MainWindow::user.userfilePath+"/"+QDateTime::currentDateTime().toString("yy-MM-dd");

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

void mainWidget::freshData(){
    if(!motorConnectFlag)return;

    minilogic->refresh();

    float force = minilogic->currentWeighValue*systemWidget::systemMap["normal"]["forceDir"].toFloat() * displayDir;
    float length = minilogic->currentMotorValue*systemWidget::systemMap["normal"]["motorDir"].toFloat() * displayDir;
    float speed = minilogic->currentSpeedValue;

    long currentTime = QDateTime::currentMSecsSinceEpoch();
    float time = float(currentTime-zreoTime)/1000;

    ui->ForceLCD->display(force);

    //此处为判断是否存在有释放按钮但电机未能响应停止而采取的补救措施
    if(!moveFlag&&abs(length-ui->lengthLCD->currentValue())>0.005){
        minilogic->motorStop();
        // qDebug()<<"补救停止";
    }
    ui->lengthLCD->display(length);

    ui->stressLCD->display(force/testDetails.sampleSquare);
    ui->strainLCD->display(length/testDetails.sampleLength);
    ui->timeLCD->display(time);

    ui->graphics->addPoint("载荷-时间",QPointF(time,force));
    ui->graphics->addPoint("位移-时间",QPointF(time,length));
    ui->graphics->addPoint("应力-应变",QPointF(length/testDetails.sampleLength,force/testDetails.sampleSquare));
    ui->graphics->addPoint("速度-时间",QPointF(time,speed));


    //最值限制
    if(abs(force)>systemWidget::systemMap["normal"]["maxForce"].toFloat()&&moveFlag){
        minilogic->motorStop();
        moveFlag = false;
        emit message("载荷超限，已自动停止");
    }
    if(abs(length)>systemWidget::systemMap["normal"]["maxMotorLength"].toFloat()&&moveFlag){
        minilogic->motorStop();
        moveFlag = false;
        emit message("电机超限，已自动停止");
    }


}

void mainWidget::on_btn_start_clicked()
{
    if(!motorConnectFlag){
        emit message("串口未连接");
        return;
    }


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

    emit message("开始测试");
    addState("开始测试");
    moveFlag = true;
}

void mainWidget::on_btn_stop_clicked()
{
    if(!motorConnectFlag){
        emit message("串口未连接");
        return;
    }

    minilogic->motorStop();
    ui->btn_start->text()="开始";
    moveFlag = false;
    emit message("测试停止");
    addState("测试停止");
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

