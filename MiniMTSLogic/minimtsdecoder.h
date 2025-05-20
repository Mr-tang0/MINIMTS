#ifndef MINIMTSDECODER_H
#define MINIMTSDECODER_H

#include <QObject>

struct MINI_MotorDetails{
    QString ID = "01";
    float resolution = 1000;
    int dirFlag = 1;    //默认电机方向为正�?
};

struct MINI_WeighDetails{
    QString ID = "02";
    float forceRate = 10;
    int forceFlag = 1;
};

struct MINI_LimitDetails{
    QString ID = "04";
    int cwLimit = 0;
    int ccwLimit = 1;
};

class MiniMTSDecoder : public QObject
{
    Q_OBJECT
public:
    explicit MiniMTSDecoder(QObject *parent = nullptr);

    float currentMotorValue = 0;
    float currentWeighValue = 0;
    float currentSpeedValue = 0;

    float trueMotorValue = 0;
    float zeroMotorValue = 0;


public slots:
    void decode(QByteArray data);
    void freshDecoderDetails(MINI_MotorDetails d1,MINI_WeighDetails d2,MINI_LimitDetails d3){
        motorDetails = d1;
        weighDetails = d2;
        limitDetails = d3;
    }

private:
    MINI_MotorDetails motorDetails;
    MINI_WeighDetails weighDetails;
    MINI_LimitDetails limitDetails;

signals:
    void LimitOccured(int axis);
    void testOK(QString msg);
};

#endif // MINIMTSDECODER_H
