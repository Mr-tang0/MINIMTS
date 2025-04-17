#include "mgmotorlogic.h"
#include "tool.h"
#include<QCoreApplication>

MGMotorLogic::MGMotorLogic(QObject *parent):
    QObject(parent)
{
    QString rootPath = QCoreApplication::applicationDirPath();
    cmdMap = Tool::readXMlFile(rootPath+"/root/system/cmd.xml")["MGMotorcmd"];

    weighMap = Tool::readXMlFile(rootPath+"/root/system/cmd.xml")["Weighcmd"];
    ioMap = Tool::readXMlFile(rootPath+"/root/system/cmd.xml")["IOcmd"];


    connect(searchTimer,&QTimer::timeout,[=](){
        QString cmd = cmdMap["readMutiLoopAngle"].arg(motorDetails.ID);
        cmd = Tool::numberVerify(cmd);
        porter->appendBuffer(cmd);

        cmd = weighMap["weighgetWeigh"].arg(weighDetails.ID);
        cmd = Tool::crc16Ccitt(cmd);
        porter->appendBuffer(cmd);

        cmd = ioMap["readInputs"].arg(limitDetails.ID);
        cmd = Tool::crc16Ccitt(cmd);
        porter->appendBuffer(cmd);

        cmd = cmdMap["readMotorState2"].arg(motorDetails.ID);
        cmd = Tool::numberVerify(cmd);
        porter->appendBuffer(cmd);
    });

    connect(porter->decoder,&decodeThread::testOK,[=](){emit testOK();});
    connect(porter->decoder,&decodeThread::motorDetails,[=](QString m){emit mDetails(m);});
    connect(porter->decoder, &decodeThread::decodeLimitOccured,this,[=](int id, int axis){
        emit LimitOccured(id, axis);
    });


}

QStringList MGMotorLogic::findPort(){
    return porter->findPort();
}

void MGMotorLogic::setOtherParam(QMap<QString,QString> params){
    weighDetails.ID = params["forceID"];
    weighDetails.forceRate = params["forceResulution"].toDouble();
    weighDetails.forceFlag = (params["forceDir"]=="1");

    limitDetails.ID = params["hardLimitID"];
    limitDetails.type = params["hardLimitAction"];
    limitDetails.enable = (params["hardLimitEnable"]=="1");
}

void MGMotorLogic::setMotorParam(QMap<QString,QString> params){


    motorDetails.ID = params["motorID"];
//    motorDetails.motorName = params["name"];

    motorDetails.angleRate = params["motorAngleRate"].toDouble();
    motorDetails.resolution = params["motorResulution"].toDouble();

//    motorDetails.forceRate = params["forceResulution"].toDouble();
    motorDetails.lengthRate = params["lengthResulution"].toDouble();

    motorDetails.maxSpeed = params["motorMaxSpeed"].toDouble();

    motorDetails.sampleRate = params["sampleRate"].toDouble();

//    if(params["motorDir"]=="1")motorDetails.dirFlag1 = true;
//    else motorDetails.dirFlag1 = false;
    motorDetails.dirFlag1 = (params["motorDir"]=="1");

    if(params["forceDir"]=="1")motorDetails.forceFlag = true;
    else motorDetails.forceFlag = false;

    if(params["lengthDir"]=="1")motorDetails.dirFlag2 = true;
    else motorDetails.dirFlag2 = false;

    weighDetails.ID = params["forceID"];


}

bool MGMotorLogic::connectMotor(QString portName, int buandRate){
    bool flag = porter->connectPort(portName,buandRate);
    if(flag)
        searchTimer->start(30);

    return flag;
}

void MGMotorLogic::disConnectMotor(){
    porter->disConnectPort();
    searchTimer->stop();
}

void MGMotorLogic::testConnect(){
    QString cmd = QStringLiteral("3E 1F %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    porter->prependBuffer(cmd);
}
void MGMotorLogic::seacherMotor(){
    QString cmd = QStringLiteral("3E 12 %1 00").arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    porter->prependBuffer(cmd);
}

void MGMotorLogic::motorOn(){
    QString cmd = cmdMap["motorOpen"].arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    porter->prependBuffer(cmd);
}

void MGMotorLogic::motorOff(){
    QString cmd = cmdMap["motorClose"].arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    porter->prependBuffer(cmd);
}

void MGMotorLogic::motorStop(){
    QString cmd = cmdMap["motorStop"].arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);
    porter->prependBuffer(cmd);
}

void MGMotorLogic::motorSpeedMove(double speed){
    float rate = speed*motorDetails.angleRate;

    QString cmd = cmdMap["speedControl"].arg(motorDetails.ID);

    cmd = Tool::numberVerify(cmd);
    QString value = Tool::numberVerify(Tool::speedToQstring(rate/motorDetails.resolution));

    cmd = cmd + " " + value;

    porter->prependBuffer(cmd);

}

void MGMotorLogic::motorLengthMove(bool Dir, double relativeLength, double speed){
    float _,currentMotorLength,__,___,absulotionLength;

    porter->getCurrentValue(&currentMotorLength,&_,&__,&___);
    currentMotorLength = currentMotorLength/motorDetails.angleRate*motorDetails.resolution;

    if(Dir){
        absulotionLength = currentMotorLength+relativeLength;
    }else absulotionLength = currentMotorLength-relativeLength;

    QString cmd = cmdMap["angleControlWithMaxSpeed"].arg(motorDetails.ID);
    cmd = Tool::numberVerify(cmd);

    absulotionLength = absulotionLength*motorDetails.angleRate;



    QString lengthString =  Tool::lengthToQstring(absulotionLength/motorDetails.resolution);

    float rate = speed*motorDetails.angleRate;
    QString speedString = Tool::speedToQstring(rate/motorDetails.resolution);

    QString dataString = Tool::numberVerify(lengthString+" "+speedString);

    cmd = cmd + " " + dataString;
    porter->prependBuffer(cmd);
}

void MGMotorLogic::getCurrentValue(float *angle,float *length,float *force, float *speed){
    float a,l,f,s;
    porter->getCurrentValue(&a,&l,&f,&s);

    *angle = a/motorDetails.angleRate*motorDetails.resolution;
    if (motorDetails.dirFlag1)*angle = -*angle;

    *length = l/motorDetails.lengthRate;
    if (motorDetails.dirFlag2)*length = -*length;

//    *force = f/motorDetails.forceRate;

    *force = f/weighDetails.forceRate;
    if (!weighDetails.forceFlag) *force = -*force;

    *speed = s/motorDetails.angleRate;
}

void MGMotorLogic::setWeighBuandRate(int buandRate){
    QString cmd = weighMap["closeWirteProtect"].arg(weighDetails.ID);
    cmd = Tool::crc16Ccitt(cmd);
    porter->appendBuffer(cmd);

    switch (buandRate) {
        case 115200:
            cmd = weighMap["writeBaudrate"].arg(weighDetails.ID).arg("08");
        break;
    }

    cmd = Tool::crc16Ccitt(cmd);
    porter->appendBuffer(cmd);
}

