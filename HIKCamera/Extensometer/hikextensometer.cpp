#include "hikextensometer.h"
#include <QDebug>
#include "imgKernal.h"
#include <opencv2/tracking.hpp>

HIKExtensometer::HIKExtensometer(QObject *parent)
    : Extensometer{parent}
{
}

void HIKExtensometer::setOrignalImg(Mat ori){
    int blur_kernel_size = 5;
    GaussianBlur(ori, ori, Size(blur_kernel_size, blur_kernel_size), 0);
    origanlImg = ori.clone();
    lastFrameMat = origanlImg.clone();

}

void HIKExtensometer::setROI(Rect roi){
    ROI = roi;
}

void HIKExtensometer::uninit(){
    if (tracker!= nullptr) {
        tracker.release();
        tracker = cv::TrackerKCF::create();
        qDebug() << "release ok";

    }
    else
    {
        tracker = cv::TrackerKCF::create();
        qDebug() << "create ok";
    }

}



void HIKExtensometer::init(){

    if (!tracker || origanlImg.empty() || ROI.empty()) {
        qDebug() << "Failed to initialize tracker.";
        return;
    }

    tracker->init(origanlImg, ROI);
    lastFrameMat = origanlImg.clone();
}

void HIKExtensometer::caculateImgAsync(const cv::Mat& img) {
    futureWatcher = new QFutureWatcher<void>(this);
    connect(futureWatcher, &QFutureWatcher<void>::finished, this, &HIKExtensometer::onCalculationFinished);

    QFuture<void> future = QtConcurrent::run([=]() {
        caculateImg(img.clone());
    });

    futureWatcher->setFuture(future);
}

cv::Mat enhanceContrast(const cv::Mat& img) {
    // 如果输入不是单通道，则先转为灰度图
    cv::Mat gray;
    if (img.channels() != 1) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = img.clone();
    }

    // 使用CLAHE提高对比度
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(4); // 设置限制阈值
    cv::Mat contrastImg;
    clahe->apply(gray, contrastImg);

    return contrastImg;
}

cv::Mat sharpenImage(const cv::Mat& img) {
    cv::Mat blurred, sharpened;
    // 高斯模糊
    cv::GaussianBlur(img, blurred, cv::Size(0, 0), 3);
    // 拉普拉斯算子
    cv::addWeighted(img, 1.5, blurred, -0.5, 0, sharpened);
    return sharpened;
}

void HIKExtensometer::caculateImg(Mat img) {

    cv::Mat roi_in_this_frame = kernel(img, cv::Point(ROI.x, ROI.y), cv::Size(ROI.width, ROI.height));

    int blur_kernel_size = 5;

    // roi_in_this_frame = enhanceContrast(roi_in_this_frame);
    GaussianBlur(roi_in_this_frame, roi_in_this_frame, cv::Size(blur_kernel_size, blur_kernel_size), 0);
    roi_in_this_frame = sharpenImage(roi_in_this_frame);
    // GaussianBlur(roi_in_this_frame, roi_in_this_frame, cv::Size(blur_kernel_size, blur_kernel_size), 0);

    cv::Point2d center = findDominantBlobCenter(roi_in_this_frame, debugFlag);



    float len = std::sqrt(std::pow(center_old.x - (ROI.x+center.x), 2) + std::pow(center_old.y - (ROI.y+center.y), 2));
    if(debugFlag)
        qDebug()<<len;

    if(len<0.02){
        emit resultEmit(center_old.x, center_old.y);
    }else
        emit resultEmit(ROI.x+center.x, ROI.y+center.y);

    if(len>1)
    {
        if(debugFlag)
            qDebug()<<QString("update:old(%1,%2),new(%3,%4),len:%5").arg(center_old.x).arg(center_old.y).arg(center.x).arg(center.y).arg(len);

        center_old = cv::Point2d(ROI.x+center.x, ROI.y+center.y);

        // 更新追踪器以定位标志点
        int i =tracker->update(img, ROI);

        if(i!=1){
            emit ROINotFound();
            qDebug()<<"ROINotFound";
        }

    }

    lastFrameMat = img.clone();

}

void HIKExtensometer::onCalculationFinished() {
    // qDebug()<<ROI.x<<ROI.y;
}


