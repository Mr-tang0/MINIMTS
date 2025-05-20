#ifndef MINIMTSLOGIC_H
#define MINIMTSLOGIC_H


#include <QObject>
#include "minimtsport.h"
#include <QTimer>

class MiniMTSLogic: public QObject
{
    Q_OBJECT
public:
    explicit MiniMTSLogic(QObject *parent = nullptr);
    // ~MiniMTSLogic();

    // 查询可用端口
    QStringList findPort();
    MiniMTSPort *getPortWin();

    bool connectDevice(const QString &portName, int buandRate, int samplerate);
    void disConnectDevice();
    void testConnect();

    void motorOn();
    void motorOff();
    void motorStop();

    void motorSpeedMove(float speed);
    void motorLengthMove(float relativeLength, float speed);

    void setChannalZero(QString channal);
    
    void setDevicesParams(MINI_MotorDetails,MINI_WeighDetails,MINI_LimitDetails);

    // 刷新数据
    void refresh();
    float currentMotorValue = 0;
    float currentWeighValue = 0;
    float currentSpeedValue = 0;



protected:
    MINI_MotorDetails motorDetails;
    MINI_WeighDetails weighDetails;
    MINI_LimitDetails limitDetails;

private:
    MiniMTSPort *port = new MiniMTSPort;
    QTimer *searchTimer = new QTimer(this);
    QTimer *searchTimer_limit = new QTimer(this);

    bool testmotor = false;
    bool testweigh = false;
    bool testlimit = false;

private slots:
    void searchPort();
    void searchPort_limit();


signals:
    void connected(bool);
    void disConnected();
    void testOK(QString);

    void LimitOccured(int axis);

};

#endif // MINIMTSLOGIC_H
