#ifndef MGMOTORLOGIC_H
#define MGMOTORLOGIC_H

#include "MGMotorLogic_global.h"
#include <QString>
#include "myport.h"
#include <QObject>
#include <QMap>
#include <QTimer>


struct MotorDetails{
    QString motorName = "";
    QString ID = "01";

    float angleRate = 1000;
    float forceRate = 10;
    float lengthRate = 10;

    float resolution = 0.01;
    float maxSpeed = 10000.00;

    int sampleRate = 25;

    bool dirFlag1 = true;//默认电机方向为正向
    bool dirFlag2 = true;//默认位移传感器方向为正向
    bool forceFlag = true;//默认力为正向

};

struct WeighDetails{
    QString ID = "02";

};

struct LoginedUSER{
    QString userName = "";
    QString password = "";
    QString role = "";
    QString userDefaultName = "";
    QString userfilePath = "";
    QString lastTestTime = "";
};




class MGMOTORLOGIC_EXPORT MGMotorLogic: public QObject
{
    Q_OBJECT
public:
    explicit MGMotorLogic(QObject *parent = nullptr);

    QStringList findPort();

    void setMotorParam(QMap<QString,QString> params);

    bool connectMotor(QString portName, int buandRate);
    void disConnectMotor();
    void testConnect();

    void motorOn();
    void motorOff();
    void motorStop();

    void motorSpeedMove(double speed);

    void motorLengthMove(bool Dir, double relativeLength, double speed);

    void getCurrentValue(float *angle,float *length,float *force, float *speed);


    void setWeighBuandRate(int buandRate);

    void clearChannal(QString channal);

    MotorDetails motorDetails;
    WeighDetails weighDetails;

private:
    QMap<QString, QString> cmdMap;
    QMap<QString, QString> weighMap;
    myPort *porter = new myPort;
    QTimer *searchTimer = new QTimer(this);



};

#endif // MGMOTORLOGIC_H
