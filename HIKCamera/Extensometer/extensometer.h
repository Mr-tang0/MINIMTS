#ifndef EXTENSOMETER_H
#define EXTENSOMETER_H

#include <QObject>
#include "imgKernal.h"
#include "opencv2/opencv.hpp"
#include <QtConcurrent>

using namespace cv;

class Extensometer : public QObject
{
    Q_OBJECT
public:
    explicit Extensometer(QObject *parent = nullptr);

    virtual void setOrignalImg(Mat ori)=0;
    virtual void setROI(Rect roi)=0;

    virtual void uninit()=0;
    virtual void init()=0;

    void debug(bool f){debugFlag = f;}
    void smooth(bool f){smoothFlag = f;};
    bool debugFlag = false;
    bool smoothFlag = true;

    Rect ROI; //感兴趣区域，粗精度，随时被修改

public slots:
    virtual void caculateImgAsync(const cv::Mat& img)=0;

private slots:
    virtual void onCalculationFinished()=0;

    virtual void caculateImg(Mat img)=0;


signals:
    void resultEmit(float x, float y);
    void ROINotFound();
};

#endif // EXTENSOMETER_H
