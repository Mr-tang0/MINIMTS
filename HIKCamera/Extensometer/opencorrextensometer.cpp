#include "opencorrextensometer.h"

#include <QDebug>
#include "imgKernal.h"
#include <opencv2/tracking.hpp>

OpenCorrExtensometer::OpenCorrExtensometer(QObject *parent)
    : Extensometer{parent}
{

}

void OpenCorrExtensometer::setOrignalImg(Mat ori){
    origanlImg = ori.clone();
    lastFrameMat = origanlImg.clone();
}

void OpenCorrExtensometer::setROI(Rect roi){
    ROI = roi;
    OLD_ROI = ROI;
    ROIF = ROI;

    int subsetSizeX = ROI.width/10;
    int subsetSizeY = ROI.width/10;

    opencorr.setLeftTop(subsetSizeX,subsetSizeY);
    opencorr.setDIC(subsetSizeX, subsetSizeY, 30, 2);

}

void OpenCorrExtensometer::uninit(){
    if (tracker!= nullptr) {
        tracker.release();
        tracker = cv::TrackerCSRT::create();
        qDebug() << "release ok";
    }
    else
    {
        tracker = cv::TrackerCSRT::create();
        qDebug() << "create ok";
    }
}



void OpenCorrExtensometer::init(){

    if (!tracker || origanlImg.empty() || ROI.empty()) {
        qDebug() << "Failed to initialize tracker.";
        return;
    }

    tracker->init(origanlImg, ROI);
    lastFrameMat = origanlImg.clone();
}

void OpenCorrExtensometer::caculateImgAsync(const cv::Mat& img) {
    QFuture<void> future = QtConcurrent::run([=]() {
        caculateImg(img.clone());
    });
}



void OpenCorrExtensometer::caculateImg(Mat img) {
    //********************************************************
    // cv::Mat ref = kernel(lastFrameMat, cv::Point(ROIF.x, ROIF.y), ROIF.size());
    // cv::Mat tar = kernel(img, cv::Point(ROIF.x, ROIF.y), ROIF.size());

    // QPointF delta = opencorr.calculate(ref, tar);  // 从 ref 到 tar 的位移

    // ROIF.x = ROIF.x+delta.x();
    // ROIF.y = ROIF.y+delta.y();

    // ROI = ROIF;

    // double coarse_center_x = ROIF.x + ROIF.width / 2.0;
    // double coarse_center_y = ROIF.y + ROIF.height / 2.0;

    // emit resultEmit(coarse_center_x, coarse_center_y);

    // ********************************************************
    // // 1. 计算 ROI 中心（粗略中心）
    // double coarse_center_x = ROI.x + ROI.width / 2.0;
    // double coarse_center_y = ROI.y + ROI.height / 2.0;

    // // 2. 提取子图（ref 和 tar）
    // cv::Mat ref = kernel(lastFrameMat, cv::Point(ROI.x, ROI.y), ROI.size());
    // cv::Mat tar = kernel(img, cv::Point(ROI.x, ROI.y), ROI.size());

    // // 3. OpenCorr 计算亚像素位移（Δx, Δy）
    // QPointF delta = opencorr.calculate(ref, tar);  // 从 ref 到 tar 的位移
    // sumX+=delta.x();
    // sumY+=delta.y();

    // // 4. 累积位移以计算标志点的实际精确位置（全局坐标）
    // double actual_x = coarse_center_x + sumX;
    // double actual_y = coarse_center_y + sumY;

    // // 发射结果
    // emit resultEmit(actual_x, actual_y);


    // // 5. 检查是否需要更新跟踪器
    // if(abs(sumX) > 2 || abs(sumY) > 2){
    //     // 更新追踪器以定位标志点
    //     int i = tracker->update(img, ROI);
    //     if(i != 1)
    //     {
    //         emit ROINotFound();
    //         qDebug() << "ROINotFound";
    //     }

    //     cv::Mat ref = kernel(lastFrameMat, cv::Point(ROI.x, ROI.y), ROI.size());
    //     cv::Mat tar = kernel(img, cv::Point(ROI.x, ROI.y), ROI.size());

    //     // 3. OpenCorr 计算亚像素位移（Δx, Δy）
    //     QPointF delta = opencorr.calculate(ref, tar);  // 从 ref 到 tar 的位移

    //     if(ROI.x!=OLD_ROI.x){
    //         if (debugFlag)
    //             qDebug() << "X" << actual_x << "," << actual_y
    //                      << "sumX=" << sumX << "sumY=" << sumY;

    //         sumX=delta.x();
    //     };
    //     if(ROI.y!=OLD_ROI.y){
    //         if (debugFlag)
    //             qDebug() << "Y" << actual_x << "," << actual_y
    //                      << "sumX=" << sumX << "sumY=" << sumY;

    //         sumY=delta.y();
    //     }

    //     OLD_ROI = ROI;
    // }

}


void OpenCorrExtensometer::onCalculationFinished() {
    // qDebug()<<ROI.x<<ROI.y;
}


