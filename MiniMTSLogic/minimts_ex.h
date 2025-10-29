#ifndef MINIMTS_EX_H
#define MINIMTS_EX_H

#include <QObject>
#include "minimtslogic.h"
#include <QQueue>
#include <QPointF>
#include "pidcontrol.h"

class MiniMTS_EX : public MiniMTSLogic
{
    Q_OBJECT
public:
    explicit MiniMTS_EX(QObject *parent = nullptr);

    void init_MINIMTSPID(QMap<QString, QString> pidMap);

    void setPID(float p, float i, float d);

    void setPIDDir(int dir);

    void setOutputRange(float min, float max);

    void setIntergralTime(float t);

    void setTargetForce(float force);

    void run(bool flag);

    void run(float target, bool flag);

    float displayOutput = 0;

    //波输出
    void setSINCurve(float amplitude, float frequency, float phase, float offset, QString type);

    //自定义输出
    void playForceSequenceWithIntervals(const QList<float> &forces, const QList<float> &intervals);
    void stopForceSequence();

    void openPIDControl(QMap<QString, QString> pidMap);

public slots:

private slots:
    void pidSlots();

    //曲线输出控制
    void updateForce();

    //自定义输出
    void onSequenceTimeout();
    void setupNextInterpolation();


private:
    PIDControl pidWin;

    // PID 参数
    float p = 0.001;
    float i = 0;
    float d = 0;
    float dt = 0.1;
    float pidDir = 1;

    float previousError, integral, derivative;

    float maxOutput = 0.1;
    float minOutput = -0.1;

    // 目标力和当前力
    float TargetForce;

    float currentWeigh; // 当前力值

    float lastForce;

    float currentMotor; // 当前位移值

    QTimer runTimer;

    //波形输出
    QString curveType = "sin";
    QTimer sinTimer;
    float sinAmplitude = 0;
    float sinFrequency = 0;
    float sinPhase = 0;
    float sinOffset = 0;

    //自定义曲线输出
    QTimer sequenceTimer;
    QList<float> forceSequence;
    QList<float> intervalSequence;
    int sequenceIndex = 0;
    bool loopEnabled = false;

    // 插值相关
    float startForce = 0;
    float endForce = 0;
    int subStep = 0;
    int totalSteps = 0;
    float stepSize = 0;
    float interpolationInterval = 0.01f; // 默认每 10ms 更新一次

signals:
    void sequenceFinished(); // 播放完成信号
};

#endif // MINIMTS_EX_H
