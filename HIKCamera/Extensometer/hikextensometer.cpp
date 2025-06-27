#include "hikextensometer.h"
#include <QDebug>
#include "imgKernal.h"


HIKExtensometer::HIKExtensometer(QObject *parent)
    : QObject{parent}
{
    tracker = cv::TrackerMIL::create(); // 创建新 tracker
}

void HIKExtensometer::setOrignalImg(Mat ori){
    int blur_kernel_size = 5;
    GaussianBlur(ori, ori, Size(blur_kernel_size, blur_kernel_size), 0);
    origanlImg = ori.clone();

}

void HIKExtensometer::setROI(Rect roi){
    ROI = roi;
}

void HIKExtensometer::uninit(){
    // if (tracker) {
    //     qDebug() << "release";
    //     tracker.release(); // 安全释放 OpenCV 的智能指针
    //     qDebug() << "release ok";
    // }
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



void HIKExtensometer::caculateImg(Mat img) {

    // sharpenCustomKernel(img);

    // 更新追踪器以定位标志点
    tracker->update(img, ROI);

    // 获取新的ROI位置
    float new_x = ROI.x;
    float new_y = ROI.y;

    // 定义四个子ROI的大小
    int sub_roi_width = ROI.width / 2;
    int sub_roi_height = ROI.height / 2;

    // 初始化变量用于存储结果
    std::vector<Point2d> centers(4);

    int blur_kernel_size = 7;
    GaussianBlur(img, img, Size(blur_kernel_size, blur_kernel_size), 0);
    GaussianBlur(img, img, Size(blur_kernel_size, blur_kernel_size), 0);
    GaussianBlur(img, img, Size(blur_kernel_size, blur_kernel_size), 0);

    // 遍历四个子区域
    for (int i = 0; i < 4; ++i) {
        // 计算子ROI的位置
        int sub_x = new_x + (i % 2) * sub_roi_width;
        int sub_y = new_y + (i / 2) * sub_roi_height;

        // 提取子ROI
        Mat this_sub_roi_in_old_frame = kernel(lastFrameMat, Point(sub_x, sub_y), Size(sub_roi_width, sub_roi_height));
        Mat this_sub_roi_in_this_frame = kernel(img, Point(sub_x, sub_y), Size(sub_roi_width, sub_roi_height));

        // 调整大小
        this_sub_roi_in_old_frame = img_resize(this_sub_roi_in_old_frame, 2);
        this_sub_roi_in_this_frame = img_resize(this_sub_roi_in_this_frame, 2);


        // 计算中心点
        Point2d center_new = FCD(this_sub_roi_in_this_frame);
        centers[i] = center_new;
    }



    double avg_dx = (centers[0].x+centers[1].x+centers[2].x+centers[3].x)/4.0;

    // 计算dys的平均值
    double avg_dy = (centers[0].y+centers[1].y+centers[2].y+centers[3].y)/4.0;

    if(debugFlag)qDebug()<<avg_dx<<avg_dy;


    Mat this_sub_roi_in_old_frame = kernel(lastFrameMat, Point(new_x, new_y), Size(120, 120));
    Mat this_sub_roi_in_this_frame = kernel(img, Point(new_x, new_y), Size(120, 120));

    double dx, dy, corr;

    // 计算PCM偏移
    PCM_shift(this_sub_roi_in_this_frame, this_sub_roi_in_old_frame, &dx, &dy, &corr);
    // 计算旧中心
    Point2d center_old = FCD(this_sub_roi_in_old_frame);
    center_old = Point2d(center_old.x+dx, center_old.y+dy);


    emit resultEmit(ROI.x+avg_dx*0.2+center_old.x*0.8, ROI.y+avg_dy*0.2+center_old.y*0.8);

    // 更新上一帧图像
    lastFrameMat = img.clone();
}

void HIKExtensometer::onCalculationFinished() {
    // qDebug()<<ROI.x<<ROI.y;
}



void HIKExtensometer::imgResolve(Mat &img){

}
