#include "imgKernal.h"

using namespace cv;


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

cv::Mat createGaussianWindow(cv::Size size, double sigma_x, double sigma_y = 0.0) {
    int rows = size.height;
    int cols = size.width;
    cv::Mat window = cv::Mat::zeros(rows, cols, CV_32F);

    int center_x = cols / 2;
    int center_y = rows / 2;

    // 如果未指定 sigma_y，则与 sigma_x 相同
    if (sigma_y == 0) {
        sigma_y = sigma_x;
    }

    double scale_x = 1.0 / (sigma_x * sigma_x);
    double scale_y = 1.0 / (sigma_y * sigma_y);

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            double dx = x - center_x;
            double dy = y - center_y;
            double exponent = -(dx * dx * scale_x + dy * dy * scale_y);
            window.at<float>(y, x) = std::exp(exponent);
        }
    }

    return window;
}

void PCM_shift(const cv::Mat &roi1, const cv::Mat &roi2, double *dx, double *dy, double *corr) {
    if (roi1.size() != roi2.size()) {
        std::cerr << "Input arrays must have the same shape." << std::endl;
        throw std::invalid_argument("Input arrays must have the same shape.");
    }

    cv::Mat gray_roi1, gray_roi2;
    if (roi1.channels() > 1)
        cvtColor(roi1, gray_roi1, cv::COLOR_BGR2GRAY);
    else
        gray_roi1 = roi1.clone();

    if (roi2.channels() > 1)
        cvtColor(roi2, gray_roi2, cv::COLOR_BGR2GRAY);
    else
        gray_roi2 = roi2.clone();

    gray_roi1.convertTo(gray_roi1, CV_32F);
    gray_roi2.convertTo(gray_roi2, CV_32F);

    cv::Mat window;
    cv::createHanningWindow(window, gray_roi1.size(), CV_32F);

    cv::Point2d point = cv::phaseCorrelate(gray_roi1, gray_roi2, window, corr);

    const double CORR_THRESHOLD = 0.1;  // 根据数据调整
    if (*corr < CORR_THRESHOLD) {
        *dx = 0;
        *dy = 0;
        *corr = 0;
        return;
    }

    *dx = point.x;
    *dy = point.y;
}

// void PCM_shift(const cv::Mat &roi1, const cv::Mat &roi2, double *dx, double *dy, double *corr) {
//     // 确保输入图像尺寸相同
//     if (roi1.size() != roi2.size()) {
//         std::cerr << "Input arrays must have the same shape." << std::endl;
//         throw std::invalid_argument("Input arrays must have the same shape.");
//     }

//     cv::Mat gray_roi1, gray_roi2;

//     // 转换为灰度图像
//     if (roi1.channels() > 1) {
//         cvtColor(roi1, gray_roi1, cv::COLOR_BGR2GRAY);
//     } else {
//         gray_roi1 = roi1.clone();
//     }
//     if (roi2.channels() > 1) {
//         cvtColor(roi2, gray_roi2, cv::COLOR_BGR2GRAY);
//     } else {
//         gray_roi2 = roi2.clone();
//     }

//     // 归一化亮度和对比度
//     cv::normalize(gray_roi1, gray_roi1, 0, 255, NORM_MINMAX);
//     cv::normalize(gray_roi2, gray_roi2, 0, 255, NORM_MINMAX);

//     // 创建汉宁窗
//     Mat window;
//     createHanningWindow(window, gray_roi1.size(), CV_32F);
//     createGaussianWindow(gray_roi1.size(), 0.4);

//     // 转换为浮点型并做零均值化
//     gray_roi1.convertTo(gray_roi1, CV_32F);
//     gray_roi2.convertTo(gray_roi2, CV_32F);

//     // 使用汉宁窗进行相位相关计算
//     Point2d point = cv::phaseCorrelate(gray_roi1, gray_roi2, window, corr);

//     *dx = point.x;
//     *dy = point.y;
// }


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

    // 全局二值化
    cv::Mat threshold_img;
    cv::threshold(gray, threshold_img, 120, 255, cv::THRESH_BINARY);

    // 反色处理，使黑色区域变为白色
    cv::Mat inverted_threshold_img;
    bitwise_not(threshold_img, inverted_threshold_img);


    // 计算图像的矩
    cv::Moments moments = cv::moments(inverted_threshold_img, true);

    Point2d center;
    // 计算几何中心
    if (moments.m00 > 0) { // 确保面积不为零避免除以零错误
        float centerX = static_cast<float>(moments.m10) / moments.m00;
        float centerY = static_cast<float>(moments.m01) / moments.m00;

        // std::cout << "黑色区域几何中心坐标: (" << centerX << ", " << centerY << ")" << std::endl;
        center = Point2d(centerX, centerY);
    }
    else {
        center = Point2d(-1, -1);
    }

    return center;

}

#include <opencv2/opencv.hpp>
using namespace cv;
using namespace std;

// Point2d findDominantBlobCenter(const cv::Mat& src, bool debug) {
//     if (src.empty()) {
//         cerr << "Image is empty!" << endl;
//         return Point2d(-1, -1);
//     }

//     cv::Mat gray, binary;

//     // Step 1: 转为灰度图
//     if (src.channels() == 3)
//         cvtColor(src, gray, COLOR_BGR2GRAY);
//     else
//         gray = src.clone();

//     cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, Size(8,8));
//     clahe->apply(gray, gray);

//     // Step 2: 自适应二值化（应对光照不均）Otsu 全局阈值
//     double otsu_thresh = threshold(gray, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

//     // 统计亮区面积
//     int white_pixels = countNonZero(binary);
//     int total_pixels = gray.total();
//     double white_ratio = (double)white_pixels / total_pixels;

//     // 如果亮区占多数，说明目标是暗的 → 用 INV
//     // 如果亮区占少数，说明目标是亮的 → 用正常
//     if (white_ratio > 0.5) {
//         threshold(gray, binary, otsu_thresh, 255, THRESH_BINARY_INV);
//     } else {
//         threshold(gray, binary, otsu_thresh, 255, THRESH_BINARY);
//     }

//     // Step 3: 形态学操作去噪（可选）
//     morphologyEx(binary, binary, MORPH_OPEN, getStructuringElement(MORPH_ELLIPSE, Size(3,3)));

//     // Step 4: 连通域分析
//     cv::Mat labels, stats, centroids;
//     int nLabels = connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);

//     if (nLabels <= 1) {
//         // 没有前景斑点
//         return Point2d(-1, -1);
//     }

//     // Step 5: 找最大面积的连通域（排除背景 label=0）
//     int maxArea = 0;
//     int bestLabel = -1;
//     for (int i = 1; i < nLabels; i++) {
//         int area = stats.at<int>(i, CC_STAT_AREA);
//         if (area > maxArea) {
//             maxArea = area;
//             bestLabel = i;
//         }
//     }

//     // 可选：设置最小面积阈值，过滤太小的噪声
//     const int minAreaThreshold = 50; // 根据图像分辨率调整
//     if (maxArea < minAreaThreshold) {
//         return Point2d(-1, -1);
//     }


//     // Step 6: 获取最大斑点的轮廓并填充孔洞
//     cv::Mat largestBlobMask = (labels == bestLabel);
//     std::vector<std::vector<cv::Point>> contours;
//     std::vector<cv::Vec4i> hierarchy;

//     findContours(largestBlobMask, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

//     // 创建一个空白图像用于绘制填充后的轮廓
//     cv::Mat filledBlob = cv::Mat::zeros(largestBlobMask.size(), CV_8UC1);

//     for (int idx = 0; idx >= 0; idx = hierarchy[idx][0]) {
//         // 绘制外部轮廓（填充）
//         drawContours(filledBlob, contours, idx, cv::Scalar(255), cv::FILLED, 8, hierarchy, 0);
//     }

//     if(debug){
//         imshow("Debug", filledBlob);
//         waitKey(5);
//     }

//     // Step 7: 计算填充后的质心

//     // Step 7: 计算填充后的质心
//     // Moments mu = moments(filledBlob, true);
//     // double centerX = mu.m10 / mu.m00;
//     // double centerY = mu.m01 / mu.m00;



//     // 假设 filledBlob 是填充后的二值图像（CV_8UC1）
//     cv::Mat blobFloat;
//     filledBlob.convertTo(blobFloat, CV_32F);

//     // 创建高斯核（大小与 blob 相同）
//     int rows = filledBlob.rows;
//     int cols = filledBlob.cols;
//     cv::Mat gaussianWeight = cv::Mat::zeros(rows, cols, CV_32F);

//     // 高斯中心设在图像中心（假设 blob 在中间附近）
//     cv::Point2f center(cols/2.0f, rows/2.0f);
//     float sigma = std::max(rows, cols) / 6.0f; // 控制权重衰减速度

//     for(int y = 0; y < rows; y++) {
//         for(int x = 0; x < cols; x++) {
//             float dx = x - center.x;
//             float dy = y - center.y;
//             gaussianWeight.at<float>(y, x) = expf(-(dx*dx + dy*dy) / (2*sigma*sigma));
//         }
//     }

//     // 加权像素值
//     cv::Mat weighted = blobFloat.mul(gaussianWeight);

//     // 计算加权质心
//     double m00 = 0, m10 = 0, m01 = 0;
//     for(int y = 0; y < rows; y++) {
//         const float* w_ptr = weighted.ptr<float>(y);
//         const float* b_ptr = blobFloat.ptr<float>(y);
//         for(int x = 0; x < cols; x++) {
//             float val = b_ptr[x];
//             if (val > 0) {
//                 m00 += w_ptr[x];
//                 m10 += x * w_ptr[x];
//                 m01 += y * w_ptr[x];
//             }
//         }
//     }

//     double centerX = (m00 > 1e-6) ? m10 / m00 : -1;
//     double centerY = (m00 > 1e-6) ? m01 / m00 : -1;


//     return Point2d(centerX, centerY);
// }

//原始代码
Point2d findDominantBlobCenter(const cv::Mat& src, bool debug) {
    if (src.empty()) {
        cerr << "Image is empty!" << endl;
        return Point2d(-1, -1);
    }

    cv::Mat gray, binary;

    // Step 1: 转为灰度图
    if (src.channels() == 3)
        cvtColor(src, gray, COLOR_BGR2GRAY);
    else
        gray = src.clone();

    // Step 2: 自适应二值化（应对光照不均）Otsu 全局阈值

    // threshold(gray, binary, 0, 255, THRESH_BINARY_INV | THRESH_OTSU);

    // Step 2: 自适应二值化（应对光照不均）Otsu 全局阈值
    double otsu_thresh = threshold(gray, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // 统计亮区面积
    int white_pixels = countNonZero(binary);
    int total_pixels = gray.total();
    double white_ratio = (double)white_pixels / total_pixels;

    // 如果亮区占多数，说明目标是暗的 → 用 INV
    // 如果亮区占少数，说明目标是亮的 → 用正常
    if (white_ratio > 0.5) {
        threshold(gray, binary, otsu_thresh, 255, THRESH_BINARY_INV);
    } else {
        threshold(gray, binary, otsu_thresh, 255, THRESH_BINARY);
    }

    if(debug){
        imshow("Debug", binary);
        waitKey(5);
    }


    // Step 3: 形态学操作去噪（可选）
    morphologyEx(binary, binary, MORPH_OPEN, getStructuringElement(MORPH_ELLIPSE, Size(3,3)));

    // Step 4: 连通域分析
    cv::Mat labels, stats, centroids;
    int nLabels = connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);

    if (nLabels <= 1) {
        // 没有前景斑点
        return Point2d(-1, -1);
    }

    // Step 5: 找最大面积的连通域（排除背景 label=0）
    int maxArea = 0;
    int bestLabel = -1;
    for (int i = 1; i < nLabels; i++) {
        int area = stats.at<int>(i, CC_STAT_AREA);
        if (area > maxArea) {
            maxArea = area;
            bestLabel = i;
        }
    }

    // 可选：设置最小面积阈值，过滤太小的噪声
    const int minAreaThreshold = 50; // 根据图像分辨率调整
    if (maxArea < minAreaThreshold) {
        return Point2d(-1, -1);
    }

    // Step 6: 获取最大斑点的质心
    cv::Mat largestBlobMask = (labels == bestLabel);
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    findContours(largestBlobMask, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

    // 创建一个空白图像用于绘制填充后的轮廓
    cv::Mat filledBlob = cv::Mat::zeros(largestBlobMask.size(), CV_8UC1);

    for (int idx = 0; idx >= 0; idx = hierarchy[idx][0]) {
        // 绘制外部轮廓（填充）
        drawContours(filledBlob, contours, idx, cv::Scalar(255), cv::FILLED, 8, hierarchy, 0);
    }

    if(debug){
        imshow("Debug", filledBlob);
        waitKey(5);
    }

    // Step 7: 计算填充后的质心
    Moments mu = moments(filledBlob, true);
    double centerX = mu.m10 / mu.m00;
    double centerY = mu.m01 / mu.m00;
    // double centerX = centroids.at<double>(bestLabel, 0);
    // double centerY = centroids.at<double>(bestLabel, 1);

    // 可视化调试（可选）

    return Point2d(centerX, centerY);
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

void sharpenCustomKernel(cv::Mat &image) {
    if(image.empty()) return;

    // 定义锐化卷积核
    float sharpen_kernel_data[] = {0, -1, 0,
                                   -1, 5, -1,
                                   0, -1, 0};
    cv::Mat sharpen_kernel(3, 3, CV_32F, sharpen_kernel_data);

    // 应用卷积核
    cv::filter2D(image, image, -1 , sharpen_kernel);
}


