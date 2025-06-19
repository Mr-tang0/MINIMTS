#include "sensortracker.h"
#include <QDebug>
#include <QDateTime>
#include <QThread>
#include <mutex>
#include "imgKernal.h"

std::mutex mtx; // 定义一个互斥锁

sensorTracker::sensorTracker(QObject *parent, int id)
	: QObject(parent)
    , m_id(id)
{
}


sensorTracker::~sensorTracker()
{}

void sensorTracker::printthread() {
	qDebug() << "thread" << QThread::currentThreadId();
}

void sensorTracker::addTrack(QString type, Mat img, Rect rect) {
	if(type == "MIL")
		tracker = TrackerMIL::create();
	else if(type == "GOTURN")
		tracker = TrackerGOTURN::create();
	else if(type == "DaSiamRPN")
		tracker = TrackerDaSiamRPN::create();
	else if (type == "Nano")
		tracker = TrackerNano::create();
	else if (type == "Vit")
        tracker = TrackerVit::create();
	else 
		tracker = TrackerMIL::create();

	
	
	tracker->init(img, rect);
	lastFrameMat = img.clone();
	roi = rect;
	setCurrentStartPlace();
}

void sensorTracker::setCurrentStartPlace() {
	Start_X = roi.x;
	Start_Y = roi.y;
}

void sensorTracker::calculate(Mat img) {
	float old_x = roi.x;
	float old_y = roi.y;
	tracker->update(img, roi); // 定位标志点

	//qDebug() << "old_x:" << old_x << "old_y:" << old_y << "w:" << roi.width<< "h:" << roi.height;

	float new_x = roi.x;
	float new_y = roi.y;
	 
	Mat this_roi_in_old_frame = kernel(lastFrameMat, Point(new_x, new_y), Size(roi.width, roi.height));
	Mat this_roi_in_this_frame = kernel(img, Point(new_x, new_y), Size(roi.width, roi.height));


	double dx, dy, corr;
	 
	this_roi_in_old_frame = img_resize(this_roi_in_old_frame, 2);
	this_roi_in_this_frame = img_resize(this_roi_in_this_frame, 2);

	PCM_shift(this_roi_in_this_frame, this_roi_in_old_frame, &dx, &dy, &corr);

	Point2d center = FCD(this_roi_in_this_frame);
	qDebug() << "center:" << center.x<< center.y;

	emit updateResult(m_id, roi.x+ center.x, roi.y + center.y);

	lastFrameMat = img.clone();

	/*qDebug() << "dx:" <<  dx << "dy:" << dy;*/

	//sum_dx -= dx / 2.0;
	//sum_dy -= dy / 2.0;

	//sum_dx+= dx;
 //   sum_dy+= dy;



	/*float step_factor = 0.05;

	float temp = abs(Start_X + sum_dx - new_x);
	if (temp > 1) {
		float adjustment = temp * step_factor;
		if (Start_X + sum_dx < new_x) {
			sum_dx += adjustment;
		}
		else {
			sum_dx -= adjustment;
		}
	}

	float temp_y = abs(Start_Y + sum_dy - new_y);
	if (temp_y > 1) {
		float adjustment = temp_y * step_factor;
		if (Start_Y + sum_dy < new_y) {
			sum_dy += adjustment;
		}
		else {
			sum_dy -= adjustment;
		}
	}*/


	//emit updateResult(m_id, Start_X + sum_dx, Start_Y + sum_dy);

	//lastFrameMat = img.clone();
}
