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
#include "MvCameraControl.h"
#include "hikextensometer.h"


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
    Rect2f A;
    Rect2f B;

    // 标志点AB的几何距离
    float A_to_B_Distense = 0.0;

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

    // 鼠标画线
    void selectLine(std::string windowtitle, Mat img);
    void selectLine(std::string windowtitle);

public slots:
    //获取相机信息
    void getCameraDetails();

    // 设置相机
    void setCamera(Camera &camera);

private:
    Ui::HIKCamera *ui;
    QMap<QString, MV_CC_DEVICE_INFO*> devices;
    Camera *camera;

    QTimer * DetectTimer = new QTimer;
    QTimer* detailsTimer = new QTimer;
    int imgheight = 640;
    int imgwidth = 480;

    QGraphicsScene* viewScene;

    QString DICFileName = "";
    QString DICFilePath = "";
    bool DICEnable = false;
    int DICCounts = 0;

    // 光学追踪
    bool trackerWorkFlagA = false;
    bool trackerWorkFlagB = false;


    HIKExtensometer HIKEA;

    HIKExtensometer HIKEB;

    float lineLength = 0;

    float A_to_B_Zreo = 0.0;
    float current_A_to_B = 0.0;


private slots:
    // 获取并显示一张图
    void catchOneFrameAndShow();

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

    void lineSeclected(double length);

};
#endif // HIKCAMERA_H
