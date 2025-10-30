#include "pyrlkextensometer.h"

#include <QDebug>
#include "imgKernal.h"
#include <opencv2/tracking.hpp>
using namespace std;

PyrLKExtensometer::PyrLKExtensometer(QObject *parent)
    : Extensometer{parent}
{}

void PyrLKExtensometer::setOrignalImg(Mat ori){
    origanlImg = ori.clone();
    lastFrameMat = origanlImg.clone();
}

void PyrLKExtensometer::setROI(Rect roi){
    ROI = roi;
    OLD_ROI = ROI;
    ROIF = ROI;
}

void PyrLKExtensometer::uninit(){

}



void PyrLKExtensometer::init(){
    lastFrameMat = origanlImg.clone();
}

void PyrLKExtensometer::caculateImgAsync(const cv::Mat& img) {
    QFuture<void> future = QtConcurrent::run([=]() {
        caculateImg(img.clone());
    });
}

bool calculateDisplacement(
    const Mat& img_prev,
    const Mat& img_next,
    const Point2f& roi_center,
    const Size& roi_size,
    Point2f& new_position,
    Point2f& displacement
    ) {

    CV_Assert(img_prev.type() == CV_8UC1 && img_next.type() == CV_8UC1);

    // 2. 定义初始特征点（ROI 中心）
    vector<Point2f> points_prev = { roi_center };

    // 3. 存储追踪结果
    vector<Point2f> points_next;
    vector<uchar>   status;
    vector<float>   err;

    TermCriteria criteria = TermCriteria(TermCriteria::COUNT + TermCriteria::EPS, 30, 0.01);

    // 5. 执行 Lucas-Kanade 光流
    calcOpticalFlowPyrLK(
        img_prev,           // 前一帧
        img_next,           // 后一帧
        points_prev,        // 输入：前一帧中的点
        points_next,        // 输出：后一帧中的对应点
        status,             // 输出：追踪状态（1=成功）
        err,                // 输出：匹配误差
        roi_size,           // 搜索窗口大小
        3,                  // 金字塔层数
        criteria            // 迭代终止条件
        );

    // 6. 检查是否追踪成功
    if (status.empty() || status[0] != 1) {
        return false;
    }

    // 7. 获取新位置
    new_position = points_next[0];

    // 8. 计算位移
    displacement = new_position - roi_center;

    return true;
}


void PyrLKExtensometer::caculateImg(Mat img) {
    Mat grayImg;
    cvtColor(img, grayImg, COLOR_BGR2GRAY);

    if (lastFrameMat.empty() || lastFrameMat.channels() == 3) {
        cvtColor(lastFrameMat, lastFrameMat, COLOR_BGR2GRAY);
    }

    double coarse_center_x = ROIF.x + ROIF.width / 2.0;
    double coarse_center_y = ROIF.y + ROIF.height / 2.0;

    Point2f roi_center(coarse_center_x, coarse_center_y);
    Size roi_size(ROIF.width, ROIF.height);

    Point2f new_pos;
    Point2f disp;

    bool success = calculateDisplacement(lastFrameMat, grayImg, roi_center, roi_size, new_pos, disp);

    if (success) {
        ROIF.x+=disp.x;
        ROIF.y+=disp.y;

        double coarse_center_x = ROIF.x + ROIF.width / 2.0;
        double coarse_center_y = ROIF.y + ROIF.height / 2.0;

        ROI = ROIF;

        emit resultEmit(coarse_center_x, coarse_center_y);
    } else {
        qDebug() << "Tracking failed!";
        emit ROINotFound();
    }

    // 更新上一帧
    grayImg.copyTo(lastFrameMat);
}


void PyrLKExtensometer::onCalculationFinished() {
    // qDebug()<<ROI.x<<ROI.y;
}
