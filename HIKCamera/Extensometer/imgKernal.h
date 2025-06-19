#pragma once
#include <QPoint>
#include "opencv2/opencv.hpp"
using namespace cv;

enum KERNAL_TYPE
{
	CIRCLE = 0,
	SQUARE = 1
};

cv::Mat img_resize(cv::Mat img, int scale_factor, InterpolationFlags flag = INTER_LINEAR);

cv::Mat gaussian_filter(const cv::Mat img, const cv::Size& kernel_size = cv::Size(3, 3), double sigmaX = 0);

void change_to_cvt_color(const cv::Mat* img);

void histMatch(const cv::Mat image1, const cv::Mat image2, cv::Mat& result, bool LUTFlag);

Mat kernel(const cv::Mat img, const cv::Point center, const cv::Size kernel);

void PCM_shift(const cv::Mat roi1, const cv::Mat roi2, double* dx, double* dy, double* corr);

Point2d FCD(const cv::Mat img);

Mat img_add_border(const cv::Mat& img, int border_size);

double ZNCC(const cv::Mat& img_old, const cv::Mat& img_new);

Mat draw_rectangle(cv::Mat img, cv::Point center, cv::Size size, cv::Scalar color, int thickness);