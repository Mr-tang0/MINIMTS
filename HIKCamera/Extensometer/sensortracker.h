#pragma once

#include <QObject>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/video/tracking.hpp>
#include <opencv2/core.hpp>
#include <QVariant>
#include <QDebug>

using namespace cv;
Q_DECLARE_METATYPE(cv::Mat)


class sensorTracker  : public QObject
{
	Q_OBJECT

public:
	sensorTracker(QObject *parent = nullptr, int id = -1);
	~sensorTracker();

	Rect roi;

	void setCurrentStartPlace();

public slots:
	void addTrack(QString type, Mat img, Rect rect);
	void processFrame(QVariant img) {
		cv::Mat frame = img.value<cv::Mat>();
		calculate(frame);
	}
	void printthread();
private:
	void calculate(Mat img);
	Ptr<cv::Tracker> tracker;


	Mat lastFrameMat;
	int m_id;

	double sum_dx = 0;
    double sum_dy = 0;

	double Start_X = 0;
	double Start_Y = 0;

signals:
	void updateResult(int id, double dx, double dy);
};
