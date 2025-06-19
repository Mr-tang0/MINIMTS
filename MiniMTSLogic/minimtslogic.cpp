#include "minimtslogic.h"
#include "tool.h"

MiniMTSLogic::MiniMTSLogic(QObject *parent):
    QObject(parent)
{
    connect(searchTimer,&QTimer::timeout,this, &MiniMTSLogic::searchPort);
    connect(searchTimer_limit,&QTimer::timeout,this,&MiniMTSLogic::searchPort_limit);
    connect(port->decoder,&MiniMTSDecoder::testOK,this,[=](QString msg){
        if(testmotor&&msg=="motor"){
            emit testOK(msg);
            testmotor = false;
        }
        if(testweigh&&msg=="weigh"){
            emit testOK(msg);
            testweigh = false;
        }
        if(testlimit&&msg=="limit"){
            emit testOK(msg);
            testlimit = false;
        }

    });

    connect(port->decoder, &MiniMTSDecoder::LimitOccured,this,&MiniMTSLogic::LimitOccured);

}

void MiniMTSLogic::searchPort(){
    //获取电机位置
    QString cmd = QString("3e 92 %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    port->appendBuffer(cmd);

    //获取称重
    cmd = QString("%1 03 00 00 00 02").arg(weighDetails.ID);
    cmd = Tool::crc16Ccitt(cmd);
    port->appendBuffer(cmd);

    //刷新数据
    // refresh();
}

void MiniMTSLogic::searchPort_limit(){
    //获取限位开关状态
    QString cmd = QString("%1 04 00 00 00 02").arg(limitDetails.ID);
    cmd = Tool::crc16Ccitt(cmd);
    port->appendBuffer(cmd);

    //获取电机状态
    cmd = QString("3e 9C %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    port->appendBuffer(cmd);
}

// 查询可用端口
QStringList MiniMTSLogic::findPort(){
    return port->findPort();
}

MiniMTSPort *MiniMTSLogic::getPortWin(){
    return port;
}

bool MiniMTSLogic::connectDevice(const QString &portName, int buandRate, int samplerate){
    bool flag = port->connectPort(portName,buandRate);
    if(flag)
    {
        int sampleDurtion = 1000.0/(samplerate)<=20 ? 20 : 1000.0/(samplerate);

        searchTimer->start(sampleDurtion);
        searchTimer_limit->start(500);
    }
    
    emit connected(flag);
    return flag;
}


void MiniMTSLogic::disConnectDevice(){
    port->disConnectPort();

    searchTimer->stop();
    searchTimer_limit->stop();

    emit disConnected();
}

void  MiniMTSLogic::testConnect(){
    QString cmd = QString("3E 1F %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    port->prependBuffer(cmd);
    testmotor = true;
    testweigh = true;
    testlimit = true;
}

void MiniMTSLogic::motorOn(){
     QString cmd = QString("3e 88 %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    port->prependBuffer(cmd);
}

void MiniMTSLogic::motorOff(){
    QString cmd = QString("3e 80 %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    port->prependBuffer(cmd);
}

void MiniMTSLogic::motorStop(){
    QString cmd = QString("3e 81 %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    port->prependBuffer(cmd);
}

void MiniMTSLogic::motorSpeedMove(float speed){

    float rate = speed * motorDetails.resolution * 100;

    QString cmd = QString("3e A2 %1 04").arg(motorDetails.ID);

    cmd = Tool::numberVerify(cmd);
    QString value = Tool::numberVerify(Tool::speedToQstring(rate));

    cmd = cmd + " " + value;

    port->prependBuffer(cmd);

}

void MiniMTSLogic::motorLengthMove(float relativeLength, float speed){
    float rate = speed * motorDetails.resolution * 100;
    float len = relativeLength * motorDetails.resolution *100 * motorDetails.dirFlag;

    QString SPD = Tool::speedToQstring(rate);
    QString INCLEN = Tool::speedToQstring(len);

    QString cmd = QString("3e A8 %1 08").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);

    QString dataString = Tool::numberVerify(INCLEN +" "+ SPD);

    cmd = cmd + " " + dataString;

    port->prependBuffer(cmd);
}

void MiniMTSLogic::setChannalZero(QString channal){
    if(channal == "motor"){
        port->decoder->zeroMotorValue = port->decoder->trueMotorValue;
    }else if (channal == "weigh")
    {
        QString cmd = QString("%1 06 00 15 00 01").arg(weighDetails.ID);
        cmd = Tool::crc16Ccitt(cmd);
        port->prependBuffer(cmd);

    }else if (channal == "encoder")
    {
        /* code */
    }
    
}

void MiniMTSLogic::setDevicesParams(MINI_MotorDetails md,MINI_WeighDetails wd,MINI_LimitDetails ld){
    motorDetails = md;
    weighDetails = wd;
    limitDetails = ld;
    port->decoder->freshDecoderDetails(md,wd,ld);
}

void MiniMTSLogic::refresh(){
    currentMotorValue = port->decoder->currentMotorValue;
    currentSpeedValue = port->decoder->currentSpeedValue;
    currentWeighValue = port->decoder->currentWeighValue;
}

