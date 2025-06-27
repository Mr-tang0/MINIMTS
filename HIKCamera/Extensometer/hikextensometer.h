#ifndef HIKEXTENSOMETER_H
#define HIKEXTENSOMETER_H

#include <QObject>
#include "imgKernal.h"
#include "opencv2/opencv.hpp"
#include <QtConcurrent>

using namespace cv;


class HIKExtensometer : public QObject
{
    Q_OBJECT
public:
    explicit HIKExtensometer(QObject *parent = nullptr);

    void setOrignalImg(Mat ori);
    void setROI(Rect roi);

    void uninit();
    void init();

    Rect ROI; //感兴趣区域，粗精度，随时被修改

    void debug(bool f){debugFlag = f;}


public slots:
    void caculateImgAsync(const cv::Mat& img);


private slots:
    void imgResolve(Mat &img); //图像预处理

    void onCalculationFinished();

private:
    Mat origanlImg; //第一张图片
    Mat lastFrameMat; //上一张图片
    Mat currentFrameMat; //当前图片


    Rect2f ROIf; //感兴趣区域，高精度

    Ptr<cv::Tracker> tracker;
    std::mutex mtx;
    std::condition_variable cv;
    bool trackingActive = false;

    void caculateImg(Mat img);
    QFutureWatcher<void> *futureWatcher;
    float oldx = 0;
    float oldy = 0;


    bool debugFlag = false;



signals:
    void resultEmit(float x, float y);
};

#endif // HIKEXTENSOMETER_H
