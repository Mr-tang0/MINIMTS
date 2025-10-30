#ifndef HIKCAMERA_H
#define HIKCAMERA_H

#include <QWidget>
#include "camera.h"
#include <QTimer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include "opencv2/opencv.hpp"
#include <QDateTime>
#include <QVariant>
#include <QPair>
#include <QPointF>
#include <QLineF>

#include "MvCameraControl.h"

#include "extensometer.h"
#include "hikextensometer.h"


#include "qtopencorr.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class HIKCamera;
}

using namespace cv;

QT_END_NAMESPACE


#ifndef BUILD_APP

#include "hikcamera_global.h"
class HIKCAMERA_EXPORT HIKCamera : public QWidget

#else

class HIKCamera : public QWidget

#endif

{
    Q_OBJECT

public:
    HIKCamera(QWidget *parent = nullptr);
    ~HIKCamera();

    static void drawRectangle(cv::Mat& image, const cv::Rect rect, const cv::Scalar& color, int thickness =1);
    static void drawRectangleAndCenter(cv::Mat& image, const cv::Rect rect, cv::Point center, const cv::Scalar& color, int thickness = 1);

    static void drawDashedLine(cv::Mat& img, cv::Point start, cv::Point end, const cv::Scalar& color,
                        int dashLength, int gapLength, int thinless);

    // 列举设备
    int EnumDevices(QMap<QString, MV_CC_DEVICE_INFO*> &ds);
    // 开启设备
    int openDevice(QString name);
    //关闭设备
    int closeDevice();
    // 是否开启
    bool isOpened();

    //开始采集数据
    void startDetect(bool detect);

    //拍摄DIC图片
    int startDIC(QString fileName,QString filePath,int duration);
    // 停止拍摄
    int stopDIC();

    //DIC距离归零
    void setCurrentZreo();

    //开启控制面板
    void enableControlWidget(bool enable);
    // 开启状态栏
    void enableStateWidget(bool enable);

    //开启连接面板
    void enableConnectWidget(bool enable);

    // 标志点AB
    cv::Point2f A;
    cv::Point2f B;


    //测试数据
    float resultCorr = 0;

    // 标志点AB的几何距离
    float A_to_B_Distense = 0.0;
    // 标志点A2B2的几何距离
    float A2_to_B2_Distense = 0.0;

    // 像素距离与物理距离的转换率
    float DistenceRate = 1.0;

    // 设置转换率
    void setDistenceRate(float v){DistenceRate = v;}

    //添加标志点 ID:A,B
    void addTracker(QString ID);

    //获取两个追踪器的状态
    void getTrackerWork(bool *flagA, bool *flagB){
        *flagA = trackerWorkFlagA;
        *flagB = trackerWorkFlagB;
    }
    // 设置两个追踪器的状态
    void startTracker(bool flagA = false, bool flagB = false){
        trackerWorkFlagA = flagA;
        trackerWorkFlagB = flagB;
    }

    //设置标距段可见
    void setBoxEnable(bool enable){boxEnable = enable;};

    // 鼠标画线绘制标定线
    void selectLine(std::string windowtitle, Mat img);
    void selectLine(std::string windowtitle);

    // 鼠标画线绘制方向
    void selectPositionLine(std::string windowtitle);

    // 鼠标画框自动识别方向
    void autoSelectPositionLine(std::string windowtitle);

    // 自动选择标记点
    void autoAddTracker(std::string windowtitle);


    // 绘制标距段端点

    // 棋盘格畸变校正

public slots:
    //获取相机信息
    void getCameraDetails();

    // 设置相机
    void setCamera(Camera &camera);

private:
    Ui::HIKCamera *ui;
    QMap<QString, MV_CC_DEVICE_INFO*> devices;
    Camera *camera;
    Mat currentFrame;

    QTimer * DetectTimer = new QTimer;
    QTimer *DICDetectTimer = new QTimer;
    QTimer* detailsTimer = new QTimer;
    int imgheight = 640;
    int imgwidth = 480;

    QGraphicsScene* viewScene;

    QString DICFileName = "";
    QString DICFilePath = "";
    int DICCounts = 0;

    // 光学追踪
    bool trackerWorkFlagA = false;
    bool trackerWorkFlagB = false;

    bool boxEnable = true;

    Extensometer *extensometerA;
    Extensometer *extensometerB;

    float lineLength = 0;

    double A_to_B_Zreo = 0.0;
    double current_A_to_B = 0.0;

    //方向和标距段保存
    QPair<cv::Point2f, cv::Point2f> boxPosition;
    QList<cv::Point2f> boxRect;
    // 绘制的标距段相对于中心点的偏移
    cv::Point2f PositionA;
    cv::Point2f PositionB;


private slots:
    // 获取并显示一张图
    void catchOneFrameAndShow();

    // DIC保存一张图
    void DICSavePIC();

    //保存一张图
    void saveImageWithTimestamp(QString fileName, QString filePath,
                                cv::Mat img, int counts);

    void on_GetParam_clicked();
    void setParamsClicked();

    void refreshPointA(float x, float y);
    void refreshPointB(float x, float y);

signals:
    void updateResult(int id, double dx, double dy);

    void trackerWork(QVariant img);

    void throwError(QString err);

    void HIKEAROI(cv::Rect);
    void HIKEBROI(cv::Rect);

    void LineSeclected(float length);

    void PositionSelected(Point2f A, Point2f B);


};
#endif // HIKCAMERA_H
