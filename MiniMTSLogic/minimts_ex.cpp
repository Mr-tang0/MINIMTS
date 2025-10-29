#include "minimts_ex.h"
#include <QDebug>
#include <cmath>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MiniMTSEx MiniMTS_EX

MiniMTS_EX::MiniMTS_EX(QObject *parent)
    : MiniMTSLogic{parent}
{
    connect(&runTimer, &QTimer::timeout,this,&MiniMTSEx::pidSlots);

    connect(&sinTimer, &QTimer::timeout, this, &MiniMTSEx::updateForce);

    connect(&sequenceTimer, &QTimer::timeout, this, &MiniMTSEx::onSequenceTimeout);

    setPID(135, 0.005, 12);
    setIntergralTime(0.2);
    setOutputRange(-0.1, 0.1);

    connect(&pidWin, &PIDControl::savePID,this,[=](float p, float i, float d, float dt, float dir, float maxo, float mino){
        setPID(p,i,d);
        setOutputRange(mino, maxo);
        setPIDDir(dir);
        setIntergralTime(dt);
    });
}

void MiniMTSEx::init_MINIMTSPID(QMap<QString, QString> pidMap){
    // PID 参数
    setPID(pidMap.value("Kp","-1").toFloat(), pidMap.value("Ki","-1").toFloat(),pidMap.value("Kd","-1").toFloat());
    setIntergralTime(pidMap.value("delta_t","null").toFloat());
    setPIDDir(pidMap.value("pidDir","null").toFloat());
    setOutputRange(pidMap.value("min_output","null").toFloat(), pidMap.value("max_output","null").toFloat());

    pidWin.initPID(pidMap);

    // qDebug()<<p<<i<<d<<dt<<pidDir<<maxOutput<<minOutput;
}

void MiniMTSEx::openPIDControl(QMap<QString, QString> pidMap){
    pidWin.initPID(pidMap);
    pidWin.show();
}

void MiniMTSEx::setPID(float p_, float i_, float d_){
    this->p = p_/10000.0;
    this->i = i_/10000.0;
    this->d = d_/10000.0;
}

void MiniMTSEx::setPIDDir(int dir){
    this->pidDir = dir;
}

void MiniMTSEx::setOutputRange(float min, float max){
    this->maxOutput = max;
    this->minOutput = min;
}

void MiniMTSEx::setIntergralTime(float t){
    this->dt = t;
}

void MiniMTSEx::setTargetForce(float force){
    TargetForce = force;
    qDebug()<<"target"<<force;
}

void MiniMTSEx::pidSlots(){
    refresh();


    currentWeigh = currentWeighValue;

    float error = TargetForce - currentWeigh;

    integral += error*dt;

    derivative = (error - previousError) / dt;

    float output = p*error + i*integral + d*derivative;


    output = output * 0.1;

    if(output>maxOutput)output = maxOutput;

    if(output<minOutput)output = minOutput;

    // qDebug()<<output<<"run"<<pidDir<<pidDir * output;
    displayOutput = pidDir * output;

    motorSpeedMove(pidDir * output);


    previousError = error;
    lastForce = currentWeigh;
}


void MiniMTSEx::run(bool flag){
    if(flag)
    {
        runTimer.start(dt*1000);
    }
    else
    {
        runTimer.stop();
        sinTimer.stop();
        stopForceSequence();
        motorStop();
    }
}

void MiniMTSEx::run(float target, bool flag){
    setTargetForce(target);
    run(flag);
}

void MiniMTSEx::setSINCurve(float amplitude, float frequency, float phase, float offset = 0, QString type = "sin") {
    sinAmplitude = amplitude;
    sinFrequency = frequency;
    sinPhase = phase;
    sinOffset = offset;
    curveType = type;

    sinTimer.start(10);
}

void MiniMTSEx::updateForce() {
    static float time = 0;
    float deltaTime = sinTimer.interval() / 1000.0f; // 转换为秒

    float angularFreq = 2 * M_PI * sinFrequency;
    float angle = angularFreq * time + sinPhase;

    float force = 0;

    if (curveType == "sin") {
        force = sinAmplitude * sin(angle) + sinOffset;
    }
    else if (curveType == "cos") {
        force = sinAmplitude * cos(angle) + sinOffset;
    }
    else if (curveType == "square") {
        // 方波：正弦函数的符号函数
        float wave = sin(angle) >= 0 ? 1.0f : -1.0f;
        force = sinAmplitude * wave + sinOffset;
    }
    else if (curveType == "triangle") {
        // 三角波：基于锯齿波的绝对值变形
        float saw = (angle / M_PI) - floor(angle / M_PI);
        float triangle = 2.0f * fabs(2.0f * saw - 1.0f) - 1.0f;
        force = sinAmplitude * triangle + sinOffset;
    }
    else {
        // 默认为正弦波
        force = sinAmplitude * sin(angle) + sinOffset;
    }

    setTargetForce(force);

    // qDebug() << "Time:" << time << "Force:" << force;

    time += deltaTime;
}

void MiniMTSEx::playForceSequenceWithIntervals(const QList<float> &forces, const QList<float> &intervals) {
    if (forces.size() < 2 || forces.size() != intervals.size() + 1) return;

    stopForceSequence();

    forceSequence = forces;
    intervalSequence = intervals;
    sequenceIndex = 0;

    // 初始化第一个段的插值
    setupNextInterpolation();

    // 固定插值间隔（比如 10ms）
    sequenceTimer.start(static_cast<int>(interpolationInterval * 1000));
}

void MiniMTSEx::stopForceSequence(){
    sequenceTimer.stop();
    sequenceIndex = 0;
}

void MiniMTSEx::setupNextInterpolation() {
    if (sequenceIndex >= forceSequence.size() - 1) return;

    startForce = forceSequence[sequenceIndex];
    endForce = forceSequence[sequenceIndex + 1];
    float duration = intervalSequence[sequenceIndex];

    totalSteps = static_cast<int>(duration / interpolationInterval);
    subStep = 0;

    if (totalSteps > 0) {
        stepSize = (endForce - startForce) / totalSteps;
    } else {
        stepSize = 0;
    }
}

void MiniMTSEx::onSequenceTimeout() {
    if (subStep >= totalSteps) {
        // 当前段完成
        setTargetForce(endForce); // 确保设置到终点值
        sequenceIndex++;

        if (sequenceIndex < forceSequence.size() - 1) {
            setupNextInterpolation();
        } else {
            if (loopEnabled) {
                sequenceIndex = 0;
                setupNextInterpolation();
            } else {
                sequenceTimer.stop();
                emit sequenceFinished();
                return;
            }
        }
    }

    float currentForce = startForce + stepSize * subStep;

    qDebug()<<currentForce;
    setTargetForce(currentForce);

    subStep++;
}
