#include "mainwidget.h"
#include "ui_mainwidget.h"
#include "systemwidget.h"
#include "xmlreader.h"
#include "mainwindow.h"

void mainWidget::keyPressEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat()) {
        if (event->key() == Qt::Key_W&&(event->modifiers() & Qt::ControlModifier)) {
            if(!motorConnectFlag){
                emit message("串口未连接");
                return;
            }

            float speed = ui->speed->value();
            minilogic->motorSpeedMove(speed);
            moveFlag = true;
            addState("正向运动中");
            keyControlMoving = true;
        }else if(event->key() == Qt::Key_S&&(event->modifiers() & Qt::ControlModifier)){
            if(!motorConnectFlag){
                emit message("串口未连接");
                return;
            }

            float speed = ui->speed->value();
            minilogic->motorSpeedMove(-speed);
            moveFlag = true;
            addState("负向运动中");
            keyControlMoving = true;
        }
    }
    QWidget::keyPressEvent(event);
}

void mainWidget::keyReleaseEvent(QKeyEvent *event){
    if (!event->isAutoRepeat()) {
        if ((event->key() == Qt::Key_W || event->key() == Qt::Key_S)&&keyControlMoving) {
            minilogic->motorStop();
            minilogic->motorStop();
            addState("停止运动");
            keyControlMoving = false;
        } else if (event->key() == Qt::Key_F5) {
            hikWin.show();
            hikWin.raise(); // 确保当前置顶
            // hikWin.activateWindow();
        }
    }

    QWidget::keyReleaseEvent(event);
}

/**
 * @brief 线性插值估值函数
 */
template<typename Func>
float estimateValue(const std::deque<DataPoint>& buffer, double targetTime, Func getVal) {
    if (buffer.empty()) return 0.0f;
    if (buffer.size() == 1) return getVal(buffer.front());

    for (size_t i = 1; i < buffer.size(); ++i) {
        const auto& p1 = buffer[i - 1];
        const auto& p2 = buffer[i];

        if (targetTime >= p1.timestamp && targetTime <= p2.timestamp) {
            double t = (targetTime - p1.timestamp) / (p2.timestamp - p1.timestamp);
            return static_cast<float>((1 - t) * getVal(p1) + t * getVal(p2));
        }
    }
    return getVal(buffer.back());
}

/**
 * @brief 核心数据刷新函数
 */
void mainWidget::freshData() {
    if (!motorConnectFlag) return;

    // 1. 硬件数据采集与物理量转换
    minilogic->refresh();

    // 从系统配置获取系数
    float forceDir = systemWidget::systemMap["normal"]["forceDir"].toFloat();
    float motorDir = systemWidget::systemMap["normal"]["motorDir"].toFloat();
    float maxForce = systemWidget::systemMap["normal"]["maxForce"].toFloat();
    float maxMotorLength = systemWidget::systemMap["normal"]["maxMotorLength"].toFloat();

    // 基础物理量计算
    float force          = minilogic->currentWeighValue * forceDir * displayDir;
    float length         = minilogic->currentMotorValue * motorDir * displayDir;
    float cameraDistance = hikWin.camera->getTrackDisplayment("A", "B") * displayDir;
    float speed          = minilogic->currentSpeedValue;
    float time           = float(QDateTime::currentMSecsSinceEpoch() - zreoTime) / 1000.0f;
    // qDebug()<<QDateTime::currentMSecsSinceEpoch()<<zreoTime<<time;

    // 应力应变计算
    float stress = force / testDetails.sampleSquare;
    float strain = length / testDetails.sampleLength;

    // 视频应变计算
    float cameraStrain = 0.0f;
    if (hikWin.camera->DistenceZreo != 0) {
        cameraStrain = cameraDistance / hikWin.camera->DistenceZreo;
    } else {
        float startVal = ui->graphics->getPointByIndex("视频引伸-时间", 0).y();
        cameraStrain = (startVal == 0) ? 0 : (cameraDistance - startVal) / startVal;
    }

    // 2. UI 仪表盘(LCD)显示更新
    ui->ForceLCD->display(force);
    ui->lengthLCD->display(length);
    ui->DICLCD->display(cameraDistance);
    ui->eStrainLCD->display(cameraStrain);
    ui->stressLCD->display(stress);
    ui->strainLCD->display(strain);
    ui->timeLCD->display(time);

    // 3. 图表曲线数据同步
    ui->graphics->addPoint("载荷-时间",     QPointF(time, force));
    ui->graphics->addPoint("位移-时间",     QPointF(time, length));
    ui->graphics->addPoint("视频引伸-时间", QPointF(time, cameraDistance));
    ui->graphics->addPoint("应力-应变",     QPointF(strain, stress));
    ui->graphics->addPoint("速度-时间",     QPointF(time, speed));
    ui->graphics->addPoint("应力-视频应变", QPointF(cameraStrain, stress));

    // 4. 安全保护与异常补救逻辑
    // 补救措施：判断是否存在释放按钮但电机未能停止的情况
    if (!moveFlag && std::abs(length - ui->lengthLCD->currentValue()) > 0.005) {
        minilogic->motorStop();
    }

    // 自动停止触发：最值超限保护
    if (moveFlag) {
        bool forceExceeded  = std::abs(force) > maxForce;
        bool lengthExceeded = std::abs(length) > maxMotorLength;

        if (forceExceeded || lengthExceeded) {
            minilogic->motorStop();
            moveFlag = false;
            QString msg = forceExceeded ? "载荷超限，已自动停止" : "电机超限，已自动停止";
            emit message(msg);
        }
    }

    // 5. 视觉跟踪数据处理 (平滑与插值估值)
    auto processTracker = [&](const QString& id, std::deque<DataPoint>& buffer, const QString& labelX, const QString& labelY) {
        cv::Point2f center = hikWin.camera->getTrackerCenter(id);

        // 更新缓存队列 (固定容量 20)
        buffer.push_back({static_cast<double>(time), center.x, center.y});
        if (buffer.size() > 20) buffer.pop_front();

        if (!buffer.empty()) {
            float estX = estimateValue(buffer, time, [](const DataPoint& p){ return p.x; });
            float estY = estimateValue(buffer, time, [](const DataPoint& p){ return p.y; });
            ui->graphics->addPoint(labelX, QPointF(time, estX));
            ui->graphics->addPoint(labelY, QPointF(time, estY));
        }
    };

    processTracker("A", historyBufferA, "AX-时间", "AY-时间");
    processTracker("B", historyBufferB, "BX-时间", "BY-时间");
}


/**
 * @brief 执行项目保存逻辑的槽函数
 * 包含：参数合法性校验 -> 内存对象更新 -> 配置文件持久化 -> UI状态同步
 */
void mainWidget::saveProject() {
    // -------------------------------------------------------
    // 1. 基础物理参数校验
    // -------------------------------------------------------
    if (ui->targetLength->value() <= 0) { emit message("目标运动距离不能为零"); return; }
    if (ui->targetSpeed->value()  <= 0) { emit message("目标运动速度不能为零"); return; }
    if (ui->size->value()         <= 0) { emit message("样品尺寸不能为零");     return; }
    if (ui->length->value()       <= 0) { emit message("样品标距段不能为零");   return; }

    // -------------------------------------------------------
    // 2. 视觉/DIC 模块特定校验
    // -------------------------------------------------------
    if (ui->DICEnable->isChecked()) {
        if (!hikWin.camera->isOpened()) { emit message("相机未连接,不可进行DIC记录"); return; }
        if (ui->DICFilePath->text().isEmpty()) { emit message("DIC图片保存位置不能为空"); return; }
        if (ui->DICFileName->text().isEmpty()) { emit message("DIC图片文件名不能为空"); return; }
        if (ui->DICDuration->value() <= 200)   { emit message("DIC采集时间不得小于200ms"); return; }
    }
    else if (ui->DICLENEnable->isChecked()) {
        if (!hikWin.camera->isOpened()) { emit message("相机未连接,不可进行视频引申计算"); return; }

        // 快捷标定处理
        if (ui->NOCalibration->isChecked()) {
            ui->pix->setText("1000");
            ui->trueLength->setText("8");
        }

        if (ui->pix->text().isEmpty() || ui->trueLength->text().isEmpty()) {
            emit message("还未进行标定"); return;
        }
        if (ui->pointA->text().isEmpty() || ui->pointB->text().isEmpty()) {
            emit message("必须选择两个标识点"); return;
        }
    }

    // -------------------------------------------------------
    // 3. 更新内存中的试验详情 (testDetails)
    // -------------------------------------------------------
    // 文本与基础设置
    testDetails.userName   = ui->userName_lineEidt->text();
    testDetails.testNumber = ui->number_lineEidt->text();
    testDetails.testTime   = ui->time_lineEidt->text();
    testDetails.testType   = ui->pull_radioButton->isChecked() ? "strech" : "compress";

    // 物理参数
    testDetails.length        = ui->targetLength->value();
    testDetails.speed         = ui->targetSpeed->value();
    testDetails.sampleSquare  = ui->size->value();
    testDetails.sampleLength  = ui->length->value();
    testDetails.autoStopType  = ui->stopAction->currentText();
    testDetails.autoStopValue = ui->stopActionValue->value();

    // 文件路径
    testDetails.userFilePath  = ui->filePath->text();
    testDetails.userFileName  = ui->fileName->text();

    // DIC/视觉详情
    testDetails.DICFileName     = ui->DICFileName->text();
    testDetails.DICFilePath     = ui->DICFilePath->text();
    testDetails.DICTriggr       = ui->DICTriggr->currentText();
    testDetails.DICDuration     = ui->DICDuration->value();
    testDetails.DICDetectEnable = ui->DICEnable->isChecked();
    testDetails.DICTriggrEnable = ui->DICTriggrEnable->isChecked();

    // 标定详情
    testDetails.pix          = ui->pix->text().toFloat();
    testDetails.trueLength   = ui->trueLength->text().toFloat();
    testDetails.DICLENEnable = ui->DICLENEnable->isChecked();

    // -------------------------------------------------------
    // 4. 同步历史配置映射 (historyMap) 用于持久化
    // -------------------------------------------------------

    // 1 name
    historyMap["index"]        = testDetails.testNumber;
    // 3 time



    // 物理参数字符串化
    historyMap["type"]         = ui->pull_radioButton->isChecked() ? "pull_radioButton" : "push_radioButton";
    historyMap["targetLength"]    = QString::number(testDetails.length);
    historyMap["targetSpeed"]     = QString::number(testDetails.speed);

    historyMap["size"]            = QString::number(testDetails.sampleSquare);
    historyMap["length"]          = QString::number(testDetails.sampleLength);
    historyMap["stopAction"]      = testDetails.autoStopType;
    historyMap["stopActionValue"] = QString::number(testDetails.autoStopValue);
    historyMap["userFilePath"] = testDetails.userFilePath;
    historyMap["userFileName"] = testDetails.userFileName;
    qDebug()<<historyMap["userFileName"];


    // 视觉设置字符串化
    historyMap["DICDetectEnable"] = testDetails.DICDetectEnable ? "true" : "false";
    historyMap["DICTriggrEnable"] = testDetails.DICTriggrEnable ? "true" : "false";
    historyMap["DICTriggr"]       = testDetails.DICTriggr;
    historyMap["DICDuration"]     = QString::number(testDetails.DICDuration);
    historyMap["DICFileName"]     = testDetails.DICFileName;
    historyMap["DICFilePath"]     = testDetails.DICFilePath;


    historyMap["DICLENEnable"]    = testDetails.DICLENEnable ? "true" : "false";
    historyMap["Calibration"]     = ui->Calibration->isChecked() ? "true" : "false";
    historyMap["trueLength"]      = QString::number(testDetails.trueLength);
    historyMap["pix"]             = QString::number(testDetails.pix);


    // -------------------------------------------------------
    // 5. 执行硬件/UI 模块的联动切换
    // -------------------------------------------------------
    if (testDetails.DICLENEnable) {
        // 开启视觉跟踪
        hikWin.camera->startTrack("A");
        hikWin.camera->startTrack("B");
        hikWin.camera->setDistenceRate(testDetails.pix / testDetails.trueLength);

        // UI 曲线与仪表盘显示
        ui->graphics->showSeries("视频引伸-时间");
        ui->graphics->showSeries("应力-视频应变");
        ui->DICLCD->show();
        ui->eStrainLCD->show();
    }
    else {
        // 关闭视觉跟踪
        hikWin.camera->stopTrack("A");
        hikWin.camera->stopTrack("B");

        // UI 曲线与仪表盘隐藏
        ui->graphics->hideSeries("应力-视频应变");
        ui->DICLCD->hide();
        ui->eStrainLCD->hide();
    }

    // -------------------------------------------------------
    // 6. 配置文件落盘与跳转
    // -------------------------------------------------------
    QMap<QString, QMap<QString, QString>> temp;
    temp["project"] = historyMap;
    XMLReader::Save_XML_File(temp, userHistroyPath);

    // 跳转到测试页并提示
    ui->btn_test->click();
    emit message("项目保存成功");
    // MainWindow::user->updateLocalDetailToCloud();
}

void mainWidget::initSystem(){
    //--------------系统设置--------------

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



    //---------------项目设置--------------
    //获取用户历史文件
    userHistroyPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +"/Tang/MiniMTS/Current/minimts_project.xml";
    historyMap = XMLReader::Read_XML_File(userHistroyPath)["project"];


    //将历史添加到表格
    //基本信息
    ui->userName_lineEidt->setText(MainWindow::user->Name);
    ui->number_lineEidt->setText(QString::number(historyMap["index"].toInt()+1));
    ui->time_lineEidt->setText(QDateTime::currentDateTime().toString("yy-MM-dd-hh:mm:ss"));

    //准静态信息
    //左
    if(historyMap["type"]=="pull_radioButton")
        ui->pull_radioButton->setChecked(true);
    else
        ui->push_radioButton->setChecked(true);
    ui->targetLength->setValue(historyMap["targetLength"].toFloat());
    ui->targetSpeed->setValue(historyMap["targetSpeed"].toFloat());

    //右
    ui->size->setValue(historyMap["size"].toFloat());
    ui->length->setValue(historyMap["length"].toFloat());
    ui->stopAction->setCurrentText(historyMap["stopAction"]);
    ui->stopActionValue->setValue(historyMap["stopActionValue"].toFloat());
    ui->filePath->setText(historyMap["userFilePath"]);
    ui->fileName->setText(historyMap["userFileName"]);


    //DIC信息
    historyMap["DICDetectEnable"]=="true" ? ui->DICEnable->setChecked(true) : ui->DICEnable->setChecked(false);
    historyMap["DICTriggrEnable"]=="true" ? ui->DICTriggrEnable->setChecked(true) : ui->DICTriggrEnable->setChecked(false);

    ui->DICTriggr->setCurrentText(historyMap["DICTriggr"]);
    ui->DICDuration->setValue(historyMap["DICDuration"].toInt());

    ui->DICFilePath->setText(historyMap["DICFilePath"]);
    ui->DICFileName->setText(historyMap["DICFileName"]);


    //视频引伸计
    historyMap["DICLENEnable"]=="true" ? ui->DICLENEnable->setChecked(true) : ui->DICLENEnable->setChecked(false);

    historyMap["Calibration"]=="true" ? ui->Calibration->setChecked(true) : ui->NOCalibration->setChecked(true);
    ui->pix->setText(historyMap["pix"]);
    ui->trueLength->setText(historyMap["trueLength"]);


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
    else
        ui->graphics->setSmooth("",1);

    //设置minimts PID
    minilogic->init_MINIMTSPID(systemWidget::systemMap["normal"]);
}

