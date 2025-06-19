#include "imgKernal.h"


cv::Mat img_resize(cv::Mat img, int scale_factor, InterpolationFlags flag) {
    cv::Size new_size(int(img.cols * scale_factor), int(img.rows * scale_factor));
    cv::resize(img, img, new_size, 0, 0, flag);
    return img;
}

cv::Mat gaussian_filter(const cv::Mat img, const cv::Size& kernel_size, double sigmaX) {
    if (kernel_size.width % 2 == 0 || kernel_size.height % 2 == 0) {
        throw std::invalid_argument("Kernel size must be an odd number.");
    }
    cv::Mat blurred_img;
    cv::GaussianBlur(img, blurred_img, kernel_size, sigmaX);
    return blurred_img;
}

void change_to_cvt_color(const cv::Mat* img) {
    if (img->channels() > 1) {
        cv::cvtColor(*img, *img, cv::COLOR_BGR2GRAY);
    }
}

void histMatch(const cv::Mat image1, const cv::Mat image2, cv::Mat& result, bool LUTFlag) {
    if (image1.size() != image2.size()) {
        std::cerr << "输入数组必须有相同的尺寸。" << std::endl;
        throw std::invalid_argument("输入数组必须有相同的尺寸。");
    }

    // 将彩色图像转换为灰度图像
    cv::Mat gray_image1, gray_image2;

    if (image1.channels() > 1)
        cvtColor(image1, gray_image1, cv::COLOR_BGR2GRAY);
    else gray_image1 = image1;

    if (image2.channels() > 1)
        cvtColor(image2, gray_image2, cv::COLOR_BGR2GRAY);
    else gray_image2 = image2;


    // 计算两个灰度图像的直方图
    cv::Mat hist_image1, hist_image2;
    int histSize = 256;
    float range[] = { 0, 256 };
    const float* histRange = { range };
    bool uniform = true, accumulate = false;
    cv::calcHist(&gray_image1, 1, 0, cv::Mat(), hist_image1, 1, &histSize, &histRange, uniform, accumulate);
    cv::calcHist(&gray_image2, 1, 0, cv::Mat(), hist_image2, 1, &histSize, &histRange, uniform, accumulate);

    // 归一化直方图
    cv::normalize(hist_image1, hist_image1, 0, 255, cv::NORM_MINMAX, -1, cv::Mat());
    cv::normalize(hist_image2, hist_image2, 0, 255, cv::NORM_MINMAX, -1, cv::Mat());

    // 计算平均亮度差
    double avg_brightness1 = cv::mean(gray_image1)[0];
    double avg_brightness2 = cv::mean(gray_image2)[0];
    double brightness_diff = avg_brightness2 - avg_brightness1;

    // 根据亮度差调整第一张图片的亮度
    result = cv::Mat(image1.size(), image1.type());
    image1.convertTo(result, -1, 1, brightness_diff);

    // 实现更完整的直方图匹配需要创建查找表（LUT）并应用到原图像上
    // 创建查找表LUT
    if(LUTFlag){
        cv::Mat LUT(1, 256, CV_8U);
        for (int i = 0; i < 256; ++i) {
            int j;
            for (j = 0; j < 256; ++j) {
                if (cv::sum(hist_image1.row(i))[0] <= cv::sum(hist_image2.row(j))[0]) break;
            }
            LUT.at<uchar>(i) = j > 0 ? j - 1 : 0;
        }
        cv::LUT(result, LUT, result);// 应用查找表
    }else {
        result = image1;
    }
}

//Mat kernel(const cv::Mat img, const cv::Point center, const cv::Size kernel ) {
//    int x1 = std::max(center.x - kernel.width / 2, 0);
//    int y1 = std::max(center.y - kernel.height / 2, 0);
//    int x2 = std::min(center.x + kernel.width / 2, img.cols);
//    int y2 = std::min(center.y + kernel.height / 2, img.rows);
//    return img(cv::Rect(x1, y1, x2 - x1, y2 - y1));
//}

cv::Mat kernel(const cv::Mat img, const cv::Point topLeft, const cv::Size kernelSize) {
    int x1 = std::max(topLeft.x, 0);
    int y1 = std::max(topLeft.y, 0);

    int x2 = std::min(topLeft.x + kernelSize.width, img.cols);
    int y2 = std::min(topLeft.y + kernelSize.height, img.rows);

    // 如果宽度或高度为0，说明超出了图像边界
    if (x2 <= x1 || y2 <= y1) {
        return cv::Mat();  // 返回空矩阵表示无效区域
    }

    return img(cv::Rect(x1, y1, x2 - x1, y2 - y1));
}


void PCM_shift(const cv::Mat roi1, const cv::Mat roi2, double *dx, double *dy, double *corr) {
    if (roi1.size() != roi2.size()) {
        std::cerr << "Input arrays must have the same shape." << std::endl;
        throw std::invalid_argument("Input arrays must have the same shape.");
    }


    cv::Mat gray_roi1, gray_roi2;
    if (roi1.channels() > 1) {
        cvtColor(roi1, gray_roi1, cv::COLOR_BGR2GRAY);
    }
    else {
        gray_roi1 = roi1.clone();
    }
    if (roi2.channels() > 1) {
        cvtColor(roi2, gray_roi2, cv::COLOR_BGR2GRAY);
    }
    else {
        gray_roi2 = roi2.clone();
    }

    //// 对比度受限的自适应直方图均衡化(CLAHE)
    //cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    //clahe->apply(gray_roi1, gray_roi1);
    //clahe->apply(gray_roi2, gray_roi2);

    //// 高斯模糊以减少高频噪声
    //GaussianBlur(gray_roi1, gray_roi1, cv::Size(3, 3), 0);
    //GaussianBlur(gray_roi2, gray_roi2, cv::Size(3, 3), 0);

    //cv::imshow("gray_roi1 ", gray_roi1);
    //cv::imshow("gray_roi2 ", gray_roi2);

    // 转换为浮点型并做零均值化
    gray_roi1.convertTo(gray_roi1, CV_32F);
    gray_roi2.convertTo(gray_roi2, CV_32F);
    //gray_roi1 -= cv::mean(gray_roi1)[0];
    //gray_roi2 -= cv::mean(gray_roi2)[0];

    // 相位相关计算
    Point2d point = cv::phaseCorrelate(gray_roi1, gray_roi2, noArray(), corr);

    *dx = point.x;
    *dy = point.y;




    //// 亚像素级峰值检测
    //auto subpixelPeak = [](const cv::Mat& matrix, const cv::Point2d& peak) -> cv::Point2d {
    //    int x = static_cast<int>(peak.x);
    //    int y = static_cast<int>(peak.y);

    //    // 边界检查
    //    if (x <= 0 || x >= matrix.cols - 1 || y <= 0 || y >= matrix.rows - 1) {
    //        return peak; // 如果在边界，则返回原始位置
    //    }

    //    double dx = (matrix.at<float>(y, x - 1) - matrix.at<float>(y, x + 1)) /
    //        (2 * matrix.at<float>(y, x - 1) - 4 * matrix.at<float>(y, x) + 2 * matrix.at<float>(y, x + 1));
    //    double dy = (matrix.at<float>(y - 1, x) - matrix.at<float>(y + 1, x)) /
    //        (2 * matrix.at<float>(y - 1, x) - 4 * matrix.at<float>(y, x) + 2 * matrix.at<float>(y + 1, x));

    //    return cv::Point2d(peak.x + dx, peak.y + dy);
    //    };

    //// 计算亚像素级别的偏移
    //cv::Mat corrMap;
    //cv::phaseCorrelate(gray_roi1, gray_roi2, cv::noArray(), corr, &corrMap);
    //point = subpixelPeak(corrMap, point);




   /* gray_roi1.convertTo(gray_roi1, CV_32F);
    gray_roi2.convertTo(gray_roi2, CV_32F);



    Point2d point = cv::phaseCorrelate(gray_roi1, gray_roi2, noArray(), corr);

    *dx = point.x;
    *dy = point.y;*/
}


Point2d FCD(const cv::Mat src) {

    if (src.empty()) {
        std::cerr << "Could not open or find the image!\n";
        return Point2d(-1, -1);
    }

    cv::Mat gray;
    // 转换为灰度图像
    if (src.channels() == 3)
        cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    else
        gray = src;

    // 高斯模糊
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

    // 全局二值化
    cv::Mat threshold_img;
    double retval = cv::threshold(gray, threshold_img, 120, 255, cv::THRESH_BINARY);

    // 反色处理，使黑色区域变为白色
    cv::Mat inverted_threshold_img;
    bitwise_not(threshold_img, inverted_threshold_img);

    // 计算图像的矩
    cv::Moments moments = cv::moments(inverted_threshold_img, true);

    Point2d center;
    // 计算几何中心（质心）
    if (moments.m00 > 0) { // 确保面积不为零避免除以零错误
        float centerX = static_cast<float>(moments.m10) / moments.m00;
        float centerY = static_cast<float>(moments.m01) / moments.m00;

        //std::cout << "黑色区域几何中心坐标: (" << centerX << ", " << centerY << ")" << std::endl;
        center = Point2d(centerX, centerY);
    }
    else {
        center = Point2d(-1, -1);
    }

    return center;

}


cv::Mat img_add_border(const cv::Mat& img, int border_size) {
    double mean_value;
    if (img.channels() == 1) {
        cv::Scalar mean_value_scalar = cv::mean(img);
        mean_value = mean_value_scalar[0];
    }
    else { 
        cv::Scalar mean_value_scalar = cv::mean(img);
        mean_value = (mean_value_scalar[0] + mean_value_scalar[1] + mean_value_scalar[2]) / 3;
    }
    cv::Mat output_img;

    cv::copyMakeBorder(img, output_img, border_size, border_size, border_size, border_size,
        cv::BORDER_CONSTANT, cv::Scalar(mean_value, mean_value, mean_value));

    return output_img;
}



double ZNCC(const cv::Mat& img_old, const cv::Mat& img_new) {
    if (img_old.size() != img_new.size()) {
        std::cerr << "Input arrays must have the same shape." << std::endl;
        throw std::invalid_argument("Input arrays must have the same shape.");
    }

    cv::Mat grey_old = img_old, grey_new = img_new;

    if (img_old.channels() > 1) {
        cv::cvtColor(img_old, grey_old, cv::COLOR_BGR2GRAY);
        cv::cvtColor(img_new, grey_new, cv::COLOR_BGR2GRAY);
    }

    double mean_old = cv::mean(grey_old)[0];
    double mean_new = cv::mean(grey_new)[0];
    cv::Scalar stddev_old, stddev_new;
    cv::meanStdDev(grey_old, cv::noArray(), stddev_old);
    cv::meanStdDev(grey_new, cv::noArray(), stddev_new);

    cv::Mat zero_mean_old = grey_old - mean_old;
    cv::Mat zero_mean_new = grey_new - mean_new;

    double A = cv::sum(zero_mean_old.mul(zero_mean_new))[0];
    double B = stddev_old[0] * stddev_new[0];

    if (B == 0) {
        return 0.0;
    }
    else {
        return std::abs(A / B);
    }
}

Mat draw_rectangle(cv::Mat img, cv::Point center, cv::Size size, cv::Scalar color, int thickness) {
    cv::Point topLeft(center.x - size.width / 2, center.y - size.height / 2);
    cv::Point bottomRight(center.x + size.width / 2, center.y + size.height / 2);

    topLeft.x = std::max(topLeft.x, 0);
    topLeft.y = std::max(topLeft.y, 0);
    bottomRight.x = std::min(bottomRight.x, img.cols);
    bottomRight.y = std::min(bottomRight.y, img.rows);

    cv::rectangle(img, topLeft, bottomRight, color, thickness);
    return img;
}

