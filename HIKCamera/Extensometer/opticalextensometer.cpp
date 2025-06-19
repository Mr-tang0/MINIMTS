#include "opticalextensometer.h"
#include "imgKernal.h"
#include <QDebug>
OpticalExtensometer::OpticalExtensometer(QObject *parent)
	: QObject(parent)
{

}

OpticalExtensometer::~OpticalExtensometer()
{}

void OpticalExtensometer::setCamera(HIKCamera* hik) {
    camera = hik;
}

void OpticalExtensometer::setRefrenceImg(Mat img, Point center, Size size) {
    orignalRoi.center = center;
    orignalRoi.size = size;
    orignalRoi.parentImg = img;

    Mat resultImg = img_resize(img, params.scale_factor, params.flag);
    resultImg = gaussian_filter(resultImg, params.gaussiankernel, params.sigmaX);

    orignalRoi.ProcessedImg = resultImg;

    orignalRoi.roi = kernel(orignalRoi.ProcessedImg, center*params.scale_factor, size * params.scale_factor);


    updateRoi = orignalRoi;

    Mat temp = draw_rectangle(img, center, size, Scalar(0, 0, 255), 2);
    cv::imshow("img", temp);
    cv::waitKey(0);
}

void OpticalExtensometer::setParams(float scale_factor, InterpolationFlags InterpolationFlag, Size gaussiankernel, double sigmaX, bool histFlag) {
    params.scale_factor = scale_factor;
    params.flag = InterpolationFlag;
    params.gaussiankernel = gaussiankernel;
    params.sigmaX = sigmaX;
    params.histFlag = histFlag;

    Mat img = img_resize(img, params.scale_factor, params.flag);
    img = gaussian_filter(img, params.gaussiankernel, params.sigmaX);

    //更新roi与处理后图像
    orignalRoi.roi = kernel(img, orignalRoi.center * params.scale_factor, orignalRoi.size * params.scale_factor);
    orignalRoi.ProcessedImg = img;
    updateRoi = orignalRoi;
}

void OpticalExtensometer::detect(Mat currentImg) {
    if (currentImg.size() != orignalRoi.parentImg.size()) {
        emit throwError("img size no match");
        qDebug() << "img size no match";
        return;
    }
    
    QDateTime startTime = QDateTime::currentDateTime();

    //使用当前图像与更新后的图像初始化
    ROI_IMG currentRoi;
    currentRoi.parentImg= currentImg;
    currentRoi.center = updateRoi.center;
    currentRoi.size = updateRoi.size;

    qDebug() << "currentRoi.ProcessedImg.size():" << currentRoi.ProcessedImg.size().height;

    //预处理输入图像
    //缩放
    currentRoi.ProcessedImg = img_resize(currentRoi.parentImg, params.scale_factor, params.flag);
    //gaoussian滤波
    currentRoi.ProcessedImg = gaussian_filter(currentRoi.ProcessedImg, params.gaussiankernel, params.sigmaX);
    ////直方图匹配 函数有问题
    //histMatch(currentRoi.ProcessedImg, updateRoi.ProcessedImg, currentRoi.ProcessedImg, params.histFlag);
    
    //获取当前图像的roi
    currentRoi.roi = kernel(currentRoi.ProcessedImg, currentRoi.center * params.scale_factor, currentRoi.size * params.scale_factor);

    double corr = 0;

    double dx = 0.0;
    double dy = 0.0;

    PCM_shift(updateRoi.roi, currentRoi.roi, &dx, &dy, &corr);

    //currentRoi.center = updateRoi.center + Point(dx, dy);
    //currentRoi.roi = kernel(currentRoi.ProcessedImg, currentRoi.center * params.scale_factor, currentRoi.size * params.scale_factor);


    updateRoi = currentRoi;
    Mat rectImg = draw_rectangle(currentRoi.parentImg, currentRoi.center, currentRoi.size, Scalar(0, 0, 255), 2);
    resize(rectImg, rectImg, Size(640, 480));
    imshow("rectImg", rectImg);
    waitKey(5);
    emit updateResult(dx, dy, corr, QDateTime::currentDateTime());
}

void OpticalExtensometer::startDetect(int mesc) {

}

void OpticalExtensometer::detectOnceFinish() {

}

void OpticalExtensometer::stopDetect() {

}


