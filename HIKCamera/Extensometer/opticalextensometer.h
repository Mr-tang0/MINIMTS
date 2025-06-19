#pragma once


#include <QObject>
#include "hikcamera.h"
#include <QTimer>
#include "opencv2/opencv.hpp"

#include <QDateTime>

using namespace cv;

struct ROI_IMG
{
	Mat roi;
    Point center;
    Size size;
	Mat parentImg;
	Mat ProcessedImg;
};

struct ExtensometerParams
{
	int scale_factor = 1;
	InterpolationFlags flag = INTER_NEAREST;
	Size gaussiankernel = {3, 3};
    double sigmaX = 0.5;
    bool histFlag = false;

};


class OpticalExtensometer  : public QObject
{
	Q_OBJECT

public:
	OpticalExtensometer(QObject *parent);
	~OpticalExtensometer();

	void setCamera(HIKCamera *hik);

	void setRefrenceImg(Mat img, Point center, Size size);
	void setParams(float scale_factor, InterpolationFlags flag, Size gaussiankernel, double sigmaX, bool histFlag);


public slots:
	void detect(Mat currentImg);

	void startDetect(int mesc);
	void detectOnceFinish();
	void stopDetect();

private:
	ROI_IMG orignalRoi;
    ROI_IMG updateRoi;

	ExtensometerParams params;


	HIKCamera *camera;
	QTimer *detectTimer = new QTimer;

private slots:
	

signals:
    void updateResult(double dx, double dy, double corr,QDateTime time);
	void throwError(QString err);
};
