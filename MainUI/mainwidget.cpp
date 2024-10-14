#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "mainwindow.h"


mainWidget::mainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::mainWidget)
{
    ui->setupUi(this);

    initChart();
    initWidget();
    initPort();
    initConnect();
    initSystem();
}

mainWidget::~mainWidget()
{
    delete ui;
    uiFreshTimer->stop();
    delete uiFreshTimer;
}

void mainWidget::initWidget(){
    //加载样式表
//    QString qss;
//    QFile file(":/qss/res/core_qss/qss/blacksoft.css");
//    if (file.open(QFile::ReadOnly)) {
//        qss = QLatin1String(file.readAll());
//        QString paletteColor = qss.mid(20, 7);
//        qApp->setPalette(QPalette(paletteColor));
//        qApp->setStyleSheet(qss);
//        file.close();
//    }

//    //先从样式表中取出对应的颜色
//    QString textColor, panelColor, borderColor, normalColorStart, normalColorEnd, darkColorStart, darkColorEnd, highColor;
//    getQssColor(qss, textColor, panelColor, borderColor, normalColorStart, normalColorEnd, darkColorStart, darkColorEnd, highColor);

//    //将对应颜色设置到控件
//    this->borderColor = highColor;
//    this->normalBgColor = normalColorStart;
//    this->darkBgColor = panelColor;
//    this->normalTextColor = textColor;
//    this->darkTextColor = normalTextColor;


    QList<int> iconsMain;
//    QList<int> iconsConfig;
//    QList<QAbstractButton *> btnsConfig;

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
    chart->legend()->hide();

    QFont font;
    font.setFamily("Arial");
    font.setPointSize(12);
    chartXAxis->setTitleText("Time (Secs)");//标题

    chartXAxis->setTitleFont(font);
    chartXAxis->setTickCount(6); //主分隔个数：0到10分成20个单位
    chartXAxis->setMinorTickCount(4); //每个单位之间绘制了多少虚网线


    chartXAxis->setLabelsFont(font);
    chartXAxis->setLabelFormat("%.2f"); //标签格式：每个单位保留几位小数


    chartYAxis->setTitleFont(font);
    chartYAxis->setTitleText("Force (N)");//标题
    chartYAxis->setTickCount(6); //主分隔个数：0到10分成20个单位
    chartYAxis->setMinorTickCount(4); //每个单位之间绘制了多少虚网线

    chartYAxis->setLabelsFont(font);
    chartYAxis->setLabelFormat("%.3f"); //标签格式：每个单位保留几位小数

    chart->addSeries(factSeries);

    ui->graphicsView->setChart(chart);


    chart->setAxisX(chartXAxis,factSeries);
    chart->setAxisY(chartYAxis,factSeries);


    ui->ForceLCD->setKey("载荷 N");
    ui->ForceLCD->setDecimals(1);

    ui->lengthLCD->setKey("理论位移 mm");
    ui->lengthLCD->setDecimals(3);

    ui->factLengthLCD->setKey("实际位移 mm");
    ui->factLengthLCD->setDecimals(3);

    ui->stressLCD->setKey("应力 MPa");
    ui->stressLCD->setDecimals(1);

    ui->strainLCD->setKey("应变");
    ui->strainLCD->setDecimals(3);

    ui->timeLCD->setKey("时长 Secs");
    ui->timeLCD->setDecimals(1);

    for (int i = 0;i<5;i++) {
        lineSeriesList.append(new QLineSeries);
        QList<float> points;
        axisPointsList.append(points);
    }

    ui->checkBox->toggle();
    zreoTime = QDateTime::currentMSecsSinceEpoch();
    zreoTime = QDateTime::currentMSecsSinceEpoch();
    freshUi();
}

void mainWidget::initPort(){
    ui->portName->clear();
    ui->portName->addItems(logic->findPort());

    ui->buandRate->clear();
    QStringList buandRateList = {"9600","19200","38400","57600","115200","230400","460800"};
    ui->buandRate->addItems(buandRateList);

}

void mainWidget::initConnect(){
    connect(ui->refresh,&QPushButton::clicked,this,[=](){initPort();emit refresh();});

    connect(ui->connect,&QPushButton::clicked,this,[=](){
        if(ui->connect->text()=="连接"){
            QString portName = ui->portName->currentText();
            int buandRate = ui->buandRate->currentText().toInt();
            motorConnectFlag = logic->connectMotor(portName,buandRate);
            if(motorConnectFlag){
                ui->connect->setText("断开");
                addStateBox("连接到串口:"+portName);
                addStateBox("波特率:"+QString::number(buandRate));
            }else addStateBox("连接失败:"+portName);
            emit connected(motorConnectFlag);

        }else{
            logic->disConnectMotor();
            ui->connect->setText("连接");
            motorConnectFlag = false;
            addStateBox("断开连接");
            emit disconnected();
        }

    });

    connect(ui->testConnect,&QPushButton::clicked,[=](){
        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }
        emit message("测试连接中");
        addStateBox("->测试连接中");

        logic->searchTimer->stop();
        logic->testConnect();
        Tool::delay(1000);
        logic->testConnect();
        Tool::delay(1000);
        logic->testConnect();
        Tool::delay(1000);
        logic->searchTimer->start();


    });
    connect(ui->motorDetails,&QPushButton::clicked,[=](){
        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }

        emit message("查询电机信息");
        addStateBox("->查询电机信息");

        logic->searchTimer->stop();
        logic->seacherMotor();
        Tool::delay(1000);
        logic->seacherMotor();
        Tool::delay(1000);
        logic->seacherMotor();
        Tool::delay(1000);
        logic->searchTimer->start();


    });
    connect(ui->toSet,&QPushButton::clicked,[=](){
        emit btnClicked("toSet");
    });
    connect(ui->clearWin,&QPushButton::clicked,[=](){
        clearStateBox();
        addStateBox("已清除");
        addStateBox("----------------");
    });


    connect(ui->speedCW,&QPushButton::pressed,[=](){
        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }

        float speed = ui->speed->value();
        logic->motorSpeedMove(speed);

    });
    connect(ui->speedCCW,&QPushButton::pressed,[=](){
        if(!motorConnectFlag){
            emit message("串口未连接");
            return;
        }

        float speed = ui->speed->value();
        logic->motorSpeedMove(-speed);
    });

    connect(ui->speedCW,&QPushButton::released,[=](){
        logic->motorStop();
    });
    connect(ui->speedCCW,&QPushButton::released,[=](){
        logic->motorStop();
    });

    connect(ui->Checked,&QPushButton::clicked,[=](){
        testDetails.userName = ui->userName_lineEidt->text();
        testDetails.testNumber = ui->number_lineEidt->text();
        testDetails.testTime = ui->time_lineEidt->text();

        if(ui->pull_radioButton->isChecked())testDetails.testType = "strech";
        else if(ui->push_radioButton->isChecked())testDetails.testType = "compress";
        testDetails.length = ui->length_doubleSpinBox->value();
        testDetails.speed = ui->speed_doubleSpinBox_2->value();

        testDetails.sampleSquare = ui->size_doubleSpinBox->value();
        testDetails.sampleLength = ui->length_doubleSpinBox_2->value();
        testDetails.autoStopType = ui->stopAction_comboBox->currentText();
        testDetails.autoStopValue = ui->stopActionValue_doubleSpinBox->value();

        testDetails.userFilePath = ui->filePath_lineEdit->text();
        testDetails.userFileName = ui->filePath_lineEdit_2->text();

        qDebug()<<testDetails.testTime;

        ui->btn_test->click();
        emit message("项目保存成功");
    });
    connect(ui->cancel,&QPushButton::clicked,[=](){
        ui->btn_test->click();
    });


    connect(uiFreshTimer,&QTimer::timeout,[=](){freshUi();});
    uiFreshTimer->start(100);

    connect(dataFreshTimer,&QTimer::timeout,[=](){freshData();});

    dataFreshTimer->start(1000/MainWindow::systemMap["normal"]["sampleRate"].toInt());

    connect(ui->lineSerialsCheck, QOverload<int, bool>::of(&QButtonGroup::buttonToggled),
          [=](int id, bool checked){
        if(checked)
        {
            currentLineSeriesID = abs(id+2);
        }
        switch (currentLineSeriesID) {
        case 0:
            chartYAxis->setTitleText("Force (N)");
            chartXAxis->setTitleText("Time (Secs)");
        break;
        case 1:
            chartYAxis->setTitleText("Stress (Mpa)");
            chartXAxis->setTitleText("Strain");
        break;
        case 2:
            chartYAxis->setTitleText("Length (mm)");
            chartXAxis->setTitleText("Time (Secs)");
        break;
        case 3:
            chartYAxis->setTitleText("Speed (mm/s)");
            chartXAxis->setTitleText("Time (Secs)");
        break;
        case 4:
            chartYAxis->setTitleText("Temp (°C)");
            chartXAxis->setTitleText("Time (Secs)");
        break;
        }
        pointsCount=0;
        freshUi();
    });

    connect(ui->ForceLCD,&MyLCD::doubleClicked,[=](){zreoForce=currentValueList[0];});
    connect(ui->lengthLCD,&MyLCD::doubleClicked,[=](){zreoLength=currentValueList[1];});
    connect(ui->factLengthLCD,&MyLCD::doubleClicked,[=](){zreoFactLength=currentValueList[2];});
    connect(ui->timeLCD,&MyLCD::doubleClicked,[=](){zreoTime=currentValueList[4];});

    connect(ui->clearChart,&QPushButton::clicked,[=](){
        for (int i = 0;i<5;i++) {
            lineSeriesList[i]->clear();
            axisPointsList[i].clear();
        }
        ui->timeLCD->doubleClick();
        chartXAxis->setRange(0,1);
        chartYAxis->setRange(0,1);

        pointsCount = 0;
        emit message("已清除");

    });
    connect(ui->setZreo,&QPushButton::clicked,[=](){
        ui->lengthLCD->doubleClick();
        ui->factLengthLCD->doubleClick();
    });

    connect(ui->chooseFilePath_pushButton, &QPushButton::clicked, this, [=](){
        QString home = MainWindow::user.userfilePath;
        QString filePath = QFileDialog::getExistingDirectory(this, tr("Select Directory"), home);
        if (!filePath.isEmpty()) {
            ui->filePath_lineEdit->setText(filePath);
        }
    });

    connect(logic,&MGMotorLogic::testOK,[=](){addStateBox("测试在线");});
    connect(logic,&MGMotorLogic::mDetails,[=](QString m){addStateBox(m);});


    connect(ui->openSampleProtect,&QRadioButton::toggled,[=](bool f){sampleProtectFlag=f;});

}

void mainWidget::initSystem(){
    //初始化连接界面
    ui->portName->setCurrentText(MainWindow::systemMap["normal"]["defaultPort"]);
    ui->buandRate->setCurrentText(MainWindow::systemMap["normal"]["defaultBuandRate"]);

    //初始化表单界面
    ui->userName_lineEidt->setText(MainWindow::user.name);
    ui->number_lineEidt->setText("0");
    ui->time_lineEidt->setText(QDateTime::currentDateTime().toString("yy-MM-dd-hh:mm:ss"));
    ui->filePath_lineEdit->setText(MainWindow::user.userfilePath);
    ui->filePath_lineEdit_2->setText(MainWindow::user.userDefaultName);


    logic->setMotorParam(MainWindow::systemMap["normal"]);
    logic->setOtherParam(MainWindow::systemMap["normal"]);

    //更新采样率
    dataFreshTimer->stop();
    dataFreshTimer->start(1000/MainWindow::systemMap["normal"]["sampleRate"].toInt());
}

void mainWidget::stopOption(bool flag, int dir){

    if (flag){
        logic->motorStop();
        logic->motorStop();
        logic->motorStop();
    }

    ui->btn_start->setEnabled(!flag);
    if (dir == 1)
        ui->speedCW->setEnabled(!flag);
    else if (dir == 2)
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
        if(axisPointsList[0].empty()){
            emit message("空数据无法保存");
            return;
        }
        QStringList header = {"force","length","fact length","speed","time"};
        bool flag = saveCsv(axisPointsList,header);
        if(flag)emit message("文件保存成功");
        else emit message("文件保存失败");
    }else if (name == "读取"){
        if(motorConnectFlag){
            emit message("连接未断开");
            return;
        }else{
            QString filePath = QFileDialog::getOpenFileName(this, "选择文件", MainWindow::user.userfilePath, "CSV Files (*.csv);;Text Files (*.txt);;All Files (*)");
            if(filePath=="")return;
            emit message("读取文件中");
            QStringList header;
            QList<QList<float>> data = Tool::loadCsvData(filePath,header);

            for (int i = 0;i<5;i++) {
                lineSeriesList[i] = new QLineSeries;
                axisPointsList[i].clear();
                axisPointsList[i]=data[i];
            }
            ui->timeLCD->doubleClick();
            chartXAxis->setRange(0,1);
            chartYAxis->setRange(0,1);

            pointsCount = 0;
            for (int i = 0;i<axisPointsList[0].length();i++) {
                *lineSeriesList[0]<<QPointF(axisPointsList[4][i],axisPointsList[0][i]);
                *lineSeriesList[1]<<QPointF(axisPointsList[0][i]/testDetails.sampleSquare,axisPointsList[2][i]/testDetails.sampleLength);
                *lineSeriesList[2]<<QPointF(axisPointsList[4][i],axisPointsList[2][i]);
                *lineSeriesList[3]<<QPointF(axisPointsList[4][i],axisPointsList[3][i]);
                *lineSeriesList[4]<<QPointF(axisPointsList[4][i],axisPointsList[0][i]);
            }

            pointsCount=0;
            freshUi();
        }
    }
}

bool mainWidget::saveCsv(QList<QList<float>> data,QList<QString> header){
    //考虑进子线程计算

    QString fileName = MainWindow::user.userDefaultName+MainWindow::systemMap["advance"]["format"];
    QString filePath = MainWindow::user.userfilePath+"/"+QDateTime::currentDateTime().toString("yy-MM-dd");

    //加入应力应变

    QList<float> stressList;
    foreach(float num,data[1]){
        stressList.append(num/testDetails.sampleSquare);
    }

    QList<float> strainList;
    foreach(float num,data[2]){
        strainList.append(abs(num/testDetails.sampleLength));
    }
    data<<stressList;
    data<<strainList;
    header<<"stress";
    header<<"strain";

    if(MainWindow::systemMap["advance"]["enableFilter"]=="yes"){
        //滤波开启
        int window = MainWindow::systemMap["advance"]["filterWindow"].toInt();
        QString type = MainWindow::systemMap["advance"]["filterMethod"];
        for (int i = 0;i<data.length();i++) {
            Tool::dataSmooth(data[i],type,window);
        }

    }
    if(MainWindow::systemMap["advance"]["enableInterpolation"]=="yes"){
        //插值开启

        int count = MainWindow::systemMap["advance"]["interpolationCount"].toInt();
        QString type = MainWindow::systemMap["advance"]["interpolationMethod"];
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

void mainWidget::freshUi(){
    if(axisPointsList.empty()||axisPointsList[0].empty())return;

    if(pointsCount==axisPointsList[0].length())
    {
        return;
    }

    pointsCount=axisPointsList[0].length();


    ui->ForceLCD->display(axisPointsList[0].last());
    ui->lengthLCD->display(axisPointsList[1].last());
    ui->factLengthLCD->display(axisPointsList[2].last());
    ui->stressLCD->display(axisPointsList[0].last()/testDetails.sampleSquare);
    ui->strainLCD->display(axisPointsList[2].last()/testDetails.sampleLength);
    ui->timeLCD->display(axisPointsList[4].last());

    while(!chart->series().isEmpty())
    {
        chart->removeSeries(chart->series().first());
    }

    *lineSeriesList[0]<<QPointF(axisPointsList[4].last(),axisPointsList[0].last());
    *lineSeriesList[1]<<QPointF(axisPointsList[0].last()/testDetails.sampleSquare,axisPointsList[2].last()/testDetails.sampleLength);
    *lineSeriesList[2]<<QPointF(axisPointsList[4].last(),axisPointsList[2].last());
    *lineSeriesList[3]<<QPointF(axisPointsList[4].last(),axisPointsList[3].last());
    *lineSeriesList[4]<<QPointF(axisPointsList[4].last(),axisPointsList[0].last());




    chart->addSeries(lineSeriesList[currentLineSeriesID]);
    float XMAX,XMIN,YMAX,YMIN;

    switch (currentLineSeriesID) {
        //force-time
        case 0:
            XMAX = Tool::max(axisPointsList[4]);
            XMIN = Tool::min(axisPointsList[4]);
            YMAX = Tool::max(axisPointsList[0]);
            YMIN = Tool::min(axisPointsList[0]);
        break;
        //应力应变
        case 1:
            XMAX = Tool::max(axisPointsList[2])/testDetails.sampleLength;
            XMIN = Tool::min(axisPointsList[2])/testDetails.sampleLength;
            YMAX = Tool::max(axisPointsList[0])/testDetails.sampleSquare;
            YMIN = Tool::min(axisPointsList[0])/testDetails.sampleSquare;
        break;
        //位移时间
        case 2:
            XMAX = Tool::max(axisPointsList[4]);
            XMIN = Tool::min(axisPointsList[4]);
            YMAX = Tool::max(axisPointsList[2]);
            YMIN = Tool::min(axisPointsList[2]);
        break;
        //速度时间
        case 3:
            XMAX = Tool::max(axisPointsList[4]);
            XMIN = Tool::min(axisPointsList[4]);
            YMAX = Tool::max(axisPointsList[3]);
            YMIN = Tool::min(axisPointsList[3]);
        break;
        //温度时间
        case 4:
            XMAX = Tool::max(axisPointsList[4]);
            XMIN = Tool::min(axisPointsList[4]);

            YMAX = Tool::max(axisPointsList[5]);
            YMIN = Tool::min(axisPointsList[5]);
        break;
    }
    chartXAxis->setRange(XMIN,XMAX);
    chartYAxis->setRange(YMIN,YMAX);


}

void mainWidget::freshData(){
    if(!motorConnectFlag)return;

    float force,length,fact,speed;
    logic->getCurrentValue(&length,&fact,&force,&speed);

    //实时滤波
    if(MainWindow::systemMap["advance"]["enableFilter_timely"]=="yes"){
        int window = MainWindow::systemMap["advance"]["filterWindow_timely"].toInt();


        if(MainWindow::systemMap["advance"]["filterMethod_timely"]=="平均加权"){
            if (axisPointsList[0].length()>window){
                for (int i = 0;i<window;i++){
                    force = force+axisPointsList[0][axisPointsList[0].length()-i-1] + zreoForce;
                }
                force = force/(window+1);
            }
        }else if(MainWindow::systemMap["advance"]["filterMethod_timely"]=="中值滤波"){
            if (axisPointsList[0].length()>window){
                QList<float> temp;
                for (int i = 0;i<window;i++){
                    temp.append(axisPointsList[0][axisPointsList[0].length()-i-1] + zreoForce);
                }
                temp.append(force);
                force = Tool::median(temp);
            }
        }
    }

    //force
    axisPointsList[0].append(force-zreoForce);

    //motorangle
    axisPointsList[1].append(length-zreoLength);

    //factlength
    fact = length;
    axisPointsList[2].append(fact-zreoFactLength);

    //speed
    axisPointsList[3].append(speed);

    //time
    long currentTime = QDateTime::currentMSecsSinceEpoch();
    float time = float(currentTime-zreoTime)/1000;
    axisPointsList[4].append(time);



    currentValueList[0]=force;
    currentValueList[1]=length;
    currentValueList[2]=fact;
    currentValueList[3]=speed;
    currentValueList[4]=currentTime;


    //自动停止
    if(startTestFlag)
    {
        qDebug()<<testDetails.autoStopType<<fact-zreoFactLength<<testDetails.autoStopValue;
        if(testDetails.autoStopType=="Fixed time(s)"){
            if(abs(time)>=testDetails.autoStopValue){
                ui->btn_stop->click();
                startTestFlag = false;
            }

        }else if(testDetails.autoStopType=="Maximum load (N)"){

            if(abs(force-zreoForce)>=testDetails.autoStopValue){
                ui->btn_stop->click();
                startTestFlag = false;
            }

        }else if(testDetails.autoStopType=="Maximum Distance(mm)"){

            if(abs(fact-zreoFactLength)>=testDetails.autoStopValue){
                ui->btn_stop->click();
                startTestFlag = false;
            }
        }
    }

    //样品保护

    if(sampleProtectFlag){
        if(force-zreoForce>5){
            float speed = 0.5;
            logic->motorSpeedMove(-speed);
        }else if(force-zreoForce<-5){
            float speed = 0.5;
            logic->motorSpeedMove(speed);
        }else{
            logic->motorStop();
        }
    }




}

void mainWidget::on_btn_start_clicked()
{
    if(!motorConnectFlag){
        emit message("串口未连接");
        return;
    }
    for (int i = 0;i<5;i++) {
        lineSeriesList[i] = new QLineSeries;
        axisPointsList[i].clear();
    }
    ui->timeLCD->doubleClick();
    chartXAxis->setRange(0,1);
    chartYAxis->setRange(0,1);

    pointsCount = 0;

    if(ui->btn_start->text()=="开始"){
        qDebug()<<testDetails.testType;
        if(testDetails.testType=="strech"){
            logic->motorLengthMove(logic->motorDetails.dirFlag1,testDetails.length,testDetails.speed);
        }else if(testDetails.testType=="compress"){

            logic->motorLengthMove(!logic->motorDetails.dirFlag1,testDetails.length,testDetails.speed);
        }
//        ui->btn_start->setText("暂停");
    }
    /*else if(ui->btn_start->text()=="暂停"){
//        logic->motorStop();
//        ui->btn_start->setText("继续");
//    }else if(ui->btn_start->text()=="继续"){
//        float tempLength = ui->lengthLCD->currentValue();
//        if(testDetails.testType=="strech"){
//            logic->motorLengthMove(logic->motorDetails.dirFlag1,testDetails.length-abs(tempLength),testDetails.speed);
//        }else if(testDetails.testType=="compress"){
//            logic->motorLengthMove(!logic->motorDetails.dirFlag1,testDetails.length-abs(tempLength),testDetails.speed);
//        }
//        ui->btn_start->setText("暂停");
//    }*/
//    startTestFlag = true;
    emit message("开始测试");
}

void mainWidget::on_btn_stop_clicked()
{
    if(!motorConnectFlag){
        emit message("串口未连接");
        return;
    }

    logic->motorStop();
    ui->btn_start->text()="开始";
    startTestFlag = false;
    emit message("测试停止");
}

void mainWidget::addStateBox(QString message){
    ui->textBrowser->append(message);
}

void mainWidget::clearStateBox(){
    ui->textBrowser->clear();

}
