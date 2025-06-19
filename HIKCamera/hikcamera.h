#ifndef HIKCAMERA_H
#define HIKCAMERA_H

#include "hikcamera_global.h"
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

QT_BEGIN_NAMESPACE
namespace Ui {
class HIKCamera;
}

using namespace cv;

QT_END_NAMESPACE

class HIKCAMERA_EXPORT HIKCamera : public QWidget
{
    Q_OBJECT

public:
    HIKCamera(QWidget *parent = nullptr);
    ~HIKCamera();

    // 查找设备
    int EnumDevices(QMap<QString, MV_CC_DEVICE_INFO*> &ds);
    int openDevice(QString name);
    int closeDevice();

    bool isOpened();

    int startDIC(QString fileName,QString filePath,int duration);
    int stopDIC();


    void enableControlWidget(bool enable);
    void enableStateWidget(bool enable);
    void enableConnectWidget(bool enable);

public slots:
    void saveImageWithTimestamp(QString fileName, QString filePath,
                                cv::Mat img, int counts);
    void calibration_clicked();
    void catchOneFrameAndShow();
    void getCameraDetails();
    void startDetect(bool detect);
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
    bool trackerWorkFlag = false;

private slots:
    void on_GetParam_clicked();
    void setParamsClicked();

signals:
    void updateResult(int id, double dx, double dy);
    void trackerWork(QVariant img);
    void throwError(QString err);
    void roiSeclected(int id,cv::Rect);
    void lineSeclected(double length);

};
#endif // HIKCAMERA_H
