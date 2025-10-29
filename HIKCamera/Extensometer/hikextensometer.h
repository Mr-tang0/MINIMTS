#ifndef HIKEXTENSOMETER_H
#define HIKEXTENSOMETER_H

#include <QObject>
#include "imgKernal.h"
#include "opencv2/opencv.hpp"
#include <QtConcurrent>
#include "extensometer.h"
using namespace cv;


class HIKExtensometer : public Extensometer
{
    Q_OBJECT
public:
    explicit HIKExtensometer(QObject *parent = nullptr);

    static void create();

    void setOrignalImg(Mat ori);
    void setROI(Rect roi);

    void uninit();
    void init();

public slots:
    void caculateImgAsync(const cv::Mat& img);

private slots:
    void onCalculationFinished();

    void caculateImg(Mat img);

private:
    Mat origanlImg; //第一张图片

    Mat lastFrameMat; //上一张图片

    Mat currentFrameMat; //当前图片

    Point2d center_old;

    Rect2f ROIf; //感兴趣区域，高精度

    Ptr<cv::Tracker> tracker;

    QFutureWatcher<void> *futureWatcher;

    float oldx = 0;
    float oldy = 0;

    bool debugFlag = false;

    bool smoothFlag = true;

};

#endif // HIKEXTENSOMETER_H
