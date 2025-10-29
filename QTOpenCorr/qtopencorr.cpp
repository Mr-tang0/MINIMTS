#include "qtopencorr.h"
#include <QDebug>

QTOpenCorr::QTOpenCorr(QObject *parent)
    : QObject{parent}
{
    setCPUThread(11);
    fftcc = new FFTCC2D(subset_radius_x, subset_radius_y, cpu_thread_number);
    icgn1 = new ICGN2D1(subset_radius_x, subset_radius_y, max_deformation_norm, max_iteration, cpu_thread_number);
}

QTOpenCorr::~QTOpenCorr(){
    delete fftcc;
    delete icgn1;
}

int QTOpenCorr::getCPUThread(){
    cpu_thread_number = omp_get_num_procs() - 1;
    omp_set_num_threads(cpu_thread_number);
    qDebug()<<"max thread:"<<cpu_thread_number;
    return cpu_thread_number;
}
void QTOpenCorr::setCPUThread(int thread){
    //set OpenMP parameters
    if(cpu_thread_number<=1)getCPUThread();
    if(thread>cpu_thread_number){
        thread = cpu_thread_number;
    }

    target_thread = thread;
    omp_set_num_threads(target_thread);
}



cv::Mat enhanceContrast(const cv::Mat& img) {
    // 如果输入不是单通道，则先转为灰度图
    cv::Mat gray;
    if (img.channels() != 1) {
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = img.clone();
    }

    // 使用CLAHE提高对比度
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(4); // 设置限制阈值
    cv::Mat contrastImg;
    clahe->apply(gray, contrastImg);

    return contrastImg;
}

cv::Mat sharpenImage(const cv::Mat& img) {
    cv::Mat blurred, sharpened;
    // 高斯模糊
    cv::GaussianBlur(img, blurred, cv::Size(0, 0), 3);
    // 拉普拉斯算子
    cv::addWeighted(img, 1.5, blurred, -0.5, 0, sharpened);
    return sharpened;
}

// 计算第三列（索引 2）的平均值，仅使用排序后处于中间三分之一的值
double calculateMiddleMean(const std::vector<std::vector<double>>& matrix, int line) {
    if (matrix.empty()) {
        return 0.0; // 或者抛出异常，取决于你的需求
    }

    // 提取第三列数据
    std::vector<double> thirdColumn;
    for (const auto& row : matrix) {
        thirdColumn.push_back(row[line]);
    }

    // 对第三列数据进行排序
    std::sort(thirdColumn.begin(), thirdColumn.end());

    // 确定中间三分之一的位置
    size_t total = thirdColumn.size();
    size_t startIdx = total / 3;
    size_t endIdx = startIdx * 2;

    // 如果总数不能被3整除，调整endIdx以确保我们取到尽可能接近1/3的数据量
    if (total % 3 != 0) {
        endIdx = std::ceil(total / 3.0) * 2;
    }

    // 计算中间三分之一的平均值
    double sum = 0.0;
    size_t count = 0;
    for (size_t i = startIdx; i < endIdx; ++i) {
        sum += thirdColumn[i];
        ++count;
    }

    return count > 0 ? sum / count : 0.0;
}

#include <QDateTime>

QPointF QTOpenCorr::calculate(cv::Mat ref, cv::Mat tar){
    // // 提高对比度
    ref = enhanceContrast(ref);
    tar = enhanceContrast(tar);

    // 锐化图像
    ref = sharpenImage(ref);
    tar = sharpenImage(tar);

    int64 start = QDateTime::currentMSecsSinceEpoch();

    cpu_thread_number = omp_get_num_procs() - 1;
    omp_set_num_threads(cpu_thread_number);



    if (ref.channels() != 1) {
        cv::cvtColor(ref, ref, cv::COLOR_BGR2GRAY);
    }
    if (tar.channels() != 1) {
        cv::cvtColor(tar, tar, cv::COLOR_BGR2GRAY);
    }

    Image2D ref_img(ref);
    Image2D tar_img(tar);

    Point2D upper_left_point(left, top);

    poi_queue.clear();

    //store POIs in a queue
    for (int i = 0; i < poi_number_y; i++)
    {
        for (int j = 0; j < poi_number_x; j++)
        {
            Point2D offset(j * grid_space, i * grid_space);
            Point2D current_point = upper_left_point + offset;
            POI2D current_poi(current_point);
            poi_queue.push_back(current_poi);
        }
    }

    // qDebug()<<"POIs"<<QDateTime::currentMSecsSinceEpoch()-start;

    start = QDateTime::currentMSecsSinceEpoch();
    fftcc->setImages(ref_img, tar_img);
    fftcc->compute(poi_queue);

    // qDebug()<<"fftcc"<<QDateTime::currentMSecsSinceEpoch()-start;

    start = QDateTime::currentMSecsSinceEpoch();
    icgn1->setImages(ref_img, tar_img);
    icgn1->prepare();
    icgn1->compute(poi_queue);

    // qDebug()<<"finish"<<QDateTime::currentMSecsSinceEpoch()-start;

    IO2D in_out; //instance for input and output DIC data
    in_out.setHeight(ref_img.height);
    in_out.setWidth(ref_img.width);

    std::vector<std::vector<double>> matrix = in_out.getDeformationTable2D(poi_queue);

    double mean2 = calculateMiddleMean(matrix, 2);
    mean2=abs(mean2)>0.01?mean2:0;
    double mean8 = calculateMiddleMean(matrix, 8);
    mean8=abs(mean8)>0.01?mean8:0;

    return QPointF(mean2, mean8);
}

QPointF QTOpenCorr::calculate(string ori, string tar){
    cv::Mat ori_ = cv::imread(ori, cv::IMREAD_GRAYSCALE);
    cv::Mat tar_ = cv::imread(tar, cv::IMREAD_GRAYSCALE);
    return calculate(ori_,tar_);
}
