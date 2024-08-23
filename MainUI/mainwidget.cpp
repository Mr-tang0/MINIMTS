#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "mainwindow.h"


mainWidget::mainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::mainWidget)
{
    ui->setupUi(this);
    logic->setWeighBuandRate(115200);

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
    chartXAxis->setLabelFormat("%.3f"); //标签格式：每个单位保留几位小数


    chartYAxis->setTitleFont(font);
    chartYAxis->setTitleText("Force (N)");//标题
    chartYAxis->setTickCount(6); //主分隔个数：0到10分成20个单位
    chartYAxis->setMinorTickCount(4); //每个单位之间绘制了多少虚网线

    chartYAxis->setLabelsFont(font);
    chartYAxis->setLabelFormat("%.2f"); //标签格式：每个单位保留几位小数

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
    ui->stressLCD->setDecimals(3);

    ui->strainLCD->setKey("应变");
    ui->strainLCD->setDecimals(3);

    ui->timeLCD->setKey("时长 Secs");
    ui->timeLCD->setDecimals(1);

    for (int i = 0;i<6;i++) {
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

    connect(ui->testConnect,&QPushButton::clicked,[=](){logic->testConnect();});
    connect(ui->motorDetails,&QPushButton::clicked,[=](){ });
    connect(ui->toSet,&QPushButton::clicked,[=](){emit btnClicked("toSet");});


    connect(ui->speedCW,&QPushButton::pressed,[=](){
        float speed = ui->speed->value();
        logic->motorSpeedMove(speed);
    });
    connect(ui->speedCCW,&QPushButton::pressed,[=](){
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
        testDetails.userFilePath = ui->filePath_lineEdit_2->text();

        ui->btn_test->click();
    });
    connect(ui->cancel,&QPushButton::clicked,[=](){
        ui->btn_test->click();
    });


    connect(uiFreshTimer,&QTimer::timeout,[=](){freshUi();});
    uiFreshTimer->start(100);

    connect(dataFreshTimer,&QTimer::timeout,[=](){freshData();});
    dataFreshTimer->start(50);

    connect(ui->lineSerialsCheck, QOverload<int, bool>::of(&QButtonGroup::buttonToggled),
          [=](int id, bool checked){
        if(checked)
        {
            currentLineSeriesID = abs(id+2);
//            chartXAxis->setRange(0,1);
//            chartYAxis->setRange(0,1);

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
    });

    connect(ui->ForceLCD,&MyLCD::doubleClicked,[=](){zreoForce=currentValueList[0];});
    connect(ui->lengthLCD,&MyLCD::doubleClicked,[=](){zreoLength=currentValueList[1];});
    connect(ui->factLengthLCD,&MyLCD::doubleClicked,[=](){zreoFactLength=currentValueList[2];});
    connect(ui->timeLCD,&MyLCD::doubleClicked,[=](){zreoTime=currentValueList[4];});

    connect(ui->clearChart,&QPushButton::clicked,[=](){
        for (int i = 0;i<6;i++) {
            lineSeriesList[i]->clear();
            axisPointsList[i].clear();
        }
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

    }else if (name == "读取"){

    }
}

void mainWidget::initPortWin(){
//    ui->portName->setCurrentText(MainWindow::systemMap[])
}

void mainWidget::initPorjectWin(){

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
    if(maxAndMinPointsList[0].empty())return;
    switch (currentLineSeriesID) {
    case 0:
        chartXAxis->setRange(maxAndMinPointsList[4][1],maxAndMinPointsList[4][0]);
        chartYAxis->setRange(maxAndMinPointsList[0][1],maxAndMinPointsList[0][0]);
    break;
    case 1:
//        chartXAxis->setRange(maxAndMinPointsList[4][1],maxAndMinPointsList[4][0]);
//        chartYAxis->setRange(maxAndMinPointsList[0][1],maxAndMinPointsList[0][0]);
    break;
    case 2:
        chartXAxis->setRange(maxAndMinPointsList[4][1],maxAndMinPointsList[4][0]);
        chartYAxis->setRange(maxAndMinPointsList[2][1],maxAndMinPointsList[2][0]);
    break;
    case 3:
        chartXAxis->setRange(maxAndMinPointsList[4][1],maxAndMinPointsList[4][0]);
        chartYAxis->setRange(maxAndMinPointsList[3][1],maxAndMinPointsList[3][0]);
    break;
    case 4:
//        chartXAxis->setRange(maxAndMinPointsList[4][1],maxAndMinPointsList[4][0]);
//        chartYAxis->setRange(maxAndMinPointsList[0][1],maxAndMinPointsList[0][0]);
    break;
    }

}

void mainWidget::freshData(){
    if(!motorConnectFlag)return;

    float force,length,fact,speed;
    logic->getCurrentValue(&length,&fact,&force,&speed);


    //force
    axisPointsList[0].append(force-zreoForce);
    maxAndMinPointsList[0][0]=force>maxAndMinPointsList[0][0]?force:maxAndMinPointsList[0][0];
    maxAndMinPointsList[0][1]=force<maxAndMinPointsList[0][1]?force:maxAndMinPointsList[0][1];

    //motorangle
    axisPointsList[1].append(length-zreoLength);
    maxAndMinPointsList[1][0]=length>maxAndMinPointsList[1][0]?length:maxAndMinPointsList[1][0];
    maxAndMinPointsList[1][1]=length<maxAndMinPointsList[1][1]?length:maxAndMinPointsList[1][1];

    //factlength
    fact = length;
    axisPointsList[2].append(fact-zreoFactLength);
    maxAndMinPointsList[2][0]=fact>maxAndMinPointsList[2][0]?fact:maxAndMinPointsList[2][0];
    maxAndMinPointsList[2][1]=fact<maxAndMinPointsList[2][1]?fact:maxAndMinPointsList[2][1];

    //speed
    axisPointsList[3].append(speed);
    maxAndMinPointsList[3][0]=speed>maxAndMinPointsList[3][0]?speed:maxAndMinPointsList[3][0];
    maxAndMinPointsList[3][1]=speed<maxAndMinPointsList[3][1]?speed:maxAndMinPointsList[3][1];

    //time
    long currentTime = QDateTime::currentMSecsSinceEpoch();
    float time = float(currentTime-zreoTime)/1000;
    axisPointsList[4].append(time);
    maxAndMinPointsList[4][0]=time>maxAndMinPointsList[4][0]?time:maxAndMinPointsList[4][0];
    maxAndMinPointsList[4][1]=time<maxAndMinPointsList[4][1]?time:maxAndMinPointsList[4][1];

    currentValueList[0]=force;
    currentValueList[1]=length;
    currentValueList[2]=fact;
    currentValueList[3]=speed;
    currentValueList[4]=currentTime;

}

void mainWidget::on_btn_start_clicked()
{
    if(ui->btn_start->text()=="开始"){
        if(testDetails.testType=="strech"){
            logic->motorLengthMove(logic->motorDetails.dirFlag1,testDetails.length,testDetails.speed);
        }else if(testDetails.testType=="compress"){
            logic->motorLengthMove(!logic->motorDetails.dirFlag1,testDetails.length,testDetails.speed);
        }
//        ui->btn_start->setText("暂停");
    }/*else if(ui->btn_start->text()=="暂停"){
        logic->motorStop();
        ui->btn_start->setText("继续");
    }else if(ui->btn_start->text()=="继续"){
        float tempLength = ui->lengthLCD->currentValue();
        if(testDetails.testType=="strech"){
            logic->motorLengthMove(logic->motorDetails.dirFlag1,testDetails.length-abs(tempLength),testDetails.speed);
        }else if(testDetails.testType=="compress"){
            logic->motorLengthMove(!logic->motorDetails.dirFlag1,testDetails.length-abs(tempLength),testDetails.speed);
        }
        ui->btn_start->setText("暂停");
    }*/
    startTestFlag = true;
}

void mainWidget::on_btn_stop_clicked()
{
    logic->motorStop();
    ui->btn_start->text()="开始";
    startTestFlag = false;
}

void mainWidget::addStateBox(QString message){
    ui->textBrowser->append(message);
}

void mainWidget::clearStateBox(){
    ui->textBrowser->clear();
}
