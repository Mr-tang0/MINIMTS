#ifndef PYRLKEXTENSOMETER_H
#define PYRLKEXTENSOMETER_H

#include <QObject>
#include "imgKernal.h"
#include "opencv2/opencv.hpp"
#include <QtConcurrent>
#include "extensometer.h"


class PyrLKExtensometer : public Extensometer
{
    Q_OBJECT
public:
    explicit PyrLKExtensometer(QObject *parent = nullptr);

    static void create();

    void setOrignalImg(cv::Mat ori);
    void setROI(cv::Rect roi);

    void uninit();
    void init();

public slots:
    void caculateImgAsync(const cv::Mat& img);


private slots:
    void onCalculationFinished();
    void caculateImg(cv::Mat img);

private:
    float totalMean = 0;

    cv::Mat origanlImg; //第一张图片

    cv::Mat lastFrameMat; //上一张图片

    cv::Mat currentFrameMat; //当前图片

    Rect OLD_ROI;
    Rect2f ROIF;

    float sumX=0;
    float sumY=0;

    QFutureWatcher<void> *futureWatcher;
};

#endif // PYRLKEXTENSOMETER_H
