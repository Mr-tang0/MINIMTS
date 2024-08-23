#include "mgmotorlogic.h"
#include "tool.h"
MGMotorLogic::MGMotorLogic(QObject *parent):
    QObject(parent)
{
    cmdMap = Tool::readXMlFile("D:/AppFile/code/qt/MINIMTS/tool/demo/cmd.xml")["MGMotorcmd"];
    weighMap = Tool::readXMlFile("D:/AppFile/code/qt/MINIMTS/tool/demo/cmd.xml")["Weighcmd"];


    connect(searchTimer,&QTimer::timeout,[=](){
        QString cmd = cmdMap["readMutiLoopAngle"].arg(motorDetails.ID);
        cmd = Tool::numberVerify(cmd);
        porter->appendBuffer(cmd);

        cmd = weighMap["weighgetWeigh"].arg(weighDetails.ID);
        cmd = Tool::crc16Ccitt(cmd);
        porter->appendBuffer(cmd);

        cmd = cmdMap["readMotorState2"].arg(motorDetails.ID);
        cmd = Tool::numberVerify(cmd);
        porter->appendBuffer(cmd);
    });

}

QStringList MGMotorLogic::findPort(){
    return porter->findPort();
}

void MGMotorLogic::setMotorParam(QMap<QString,QString> params){


    motorDetails.ID = params["motorID"];
//    motorDetails.motorName = params["name"];

    motorDetails.angleRate = params["motorAngleRate"].toDouble();
    motorDetails.resolution = params["motorResulution"].toDouble();

    motorDetails.forceRate = params["forceResulution"].toDouble();
    motorDetails.lengthRate = params["lengthResulution"].toDouble();

    motorDetails.maxSpeed = params["motorMaxSpeed"].toDouble();

    motorDetails.sampleRate = params["sampleRate"].toDouble();

    if(params["motorDir"]=="1")motorDetails.dirFlag1 = true;
    else motorDetails.dirFlag1 = false;

    if(params["forceDir"]=="1")motorDetails.forceFlag = true;
    else motorDetails.forceFlag = false;

    if(params["lengthDir"]=="1")motorDetails.dirFlag2 = true;
    else motorDetails.dirFlag2 = false;

    weighDetails.ID = params["forceID"];


}

bool MGMotorLogic::connectMotor(QString portName, int buandRate){
    bool flag = porter->connectPort(portName,buandRate);
    if(flag)
        searchTimer->start(1000/motorDetails.sampleRate);

    return flag;
}

void MGMotorLogic::disConnectMotor(){
    porter->disConnectPort();
    searchTimer->stop();
}

void MGMotorLogic::testConnect(){
    QString id = QStringLiteral("0%1");
    QString cmd = "3E 1F %1 00";
    for(int i = 1;i<10;i++){
        id = id.arg(i);
        cmd = cmd.arg(id);
        cmd = Tool::numberVerify(cmd);
        porter->prependBuffer(cmd);
        cmd = "3E 1F %1 00";
        id = QStringLiteral("0%1");
    }
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
    qDebug()<<cmd;

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
    qDebug()<<cmd;
    porter->prependBuffer(cmd);
}

void MGMotorLogic::getCurrentValue(float *angle,float *length,float *force, float *speed){
    float a,l,f,s;
    porter->getCurrentValue(&a,&l,&f,&s);


    *angle = a/motorDetails.angleRate*motorDetails.resolution;
    *length = l/motorDetails.lengthRate;
    *force = f/motorDetails.forceRate;
    *speed = s/motorDetails.angleRate*motorDetails.resolution;
}

void MGMotorLogic::setWeighBuandRate(int buandRate){
    QString cmd = weighMap["closeWirteProtect"].arg(weighDetails.ID);
    cmd = Tool::crc16Ccitt(cmd);
    qDebug()<<cmd;
//    porter->appendBuffer(cmd);

    switch (buandRate) {
        case 115200:
            cmd = weighMap["writeBaudrate"].arg(weighDetails.ID).arg("08");
        break;
    }

    cmd = Tool::crc16Ccitt(cmd);
    qDebug()<<cmd;
//    porter->appendBuffer(cmd);
}

