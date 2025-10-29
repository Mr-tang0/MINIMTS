#include "hikcamera.h"
#include "ui_hikcamera.h"
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QtConcurrent>
#include <QScreen>
#include <QThread>
#include "imgKernal.h"

#include "hikextensometer.h"
#include "opencorrextensometer.h"
#include "pyrlkextensometer.h"


HIKCamera::HIKCamera(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HIKCamera)
{
    ui->setupUi(this);

    camera = MV_NULL;

#ifndef BUILD_APP
    enableControlWidget(false);
    enableConnectWidget(false);

#else
    enableControlWidget(true);
    enableConnectWidget(true);
#endif

    extensometerA = new PyrLKExtensometer;
    extensometerB = new PyrLKExtensometer;


    connect(DetectTimer, &QTimer::timeout, this, &HIKCamera::catchOneFrameAndShow);
    connect(DICDetectTimer, &QTimer::timeout, this, &HIKCamera::DICSavePIC);

    connect(detailsTimer, &QTimer::timeout, this, &HIKCamera::getCameraDetails);

    connect(ui->SetParam, &QPushButton::clicked, this, &HIKCamera::setParamsClicked);

    connect(ui->setupCamera, &QPushButton::clicked, this, [=]() {
        if(ui->setupCamera->text()=="相机设置")
        {
            enableControlWidget(true);
            ui->setupCamera->setText("关闭设置");
        }else{
            enableControlWidget(false);
            ui->setupCamera->setText("相机设置");
        }
    });

    viewScene = ui->graphicsView->scene();
    if (!viewScene) {
        viewScene = new QGraphicsScene(this);
        ui->graphicsView->setScene(viewScene);
    }

    connect(ui->refresh,&QPushButton::clicked,this,[=](){
        ui->camera->clear();
        devices.clear();
        Camera::EnumDevices(devices);
        ui->camera->addItems(devices.keys());
    });

    connect(ui->connect,&QPushButton::clicked,this,[=](){
        if(ui->connect->text()=="open")
        {
            QString device = ui->camera->currentText();
            camera = new Camera;
            int ret = camera->Open(devices[device]);
            if(ret == MV_OK){
                qDebug()<<"打开成功";
                ui->connect->setText("close");

                int ret = camera->StartGrabbing();
                if(ret==MV_OK){
                    startDetect(true);
                }else{
                    QMessageBox::information(this,"异常","采集设备失败");
                }
            }else QMessageBox::information(this,"异常","打开设备失败");
        }else{
            startDetect(false);
            camera->StopGrabbing();
            camera->Close();
            ui->connect->setText("open");
        }

    });

    connect(extensometerA,&Extensometer::resultEmit,this,&HIKCamera::refreshPointA);
    connect(extensometerB,&Extensometer::resultEmit,this,&HIKCamera::refreshPointB);

    ui->refresh->click();
}

HIKCamera::~HIKCamera()
{
    qDebug()<<"widget close";
    if(camera!=MV_NULL)
    {
        camera->StopGrabbing();
        camera->Close();
    }
    delete ui;
}

void HIKCamera::saveImageWithTimestamp(QString fileName, QString filePath,
                                       cv::Mat img, int counts)
{
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();

    QString countStr = QString("%1").arg(counts, 8, 10, QChar('0'));

    QString finalFileName = QString("%1_%2_%3.png")
                                .arg(fileName)
                                .arg(countStr)
                                .arg(timestamp);

    QString fullPath = filePath;
    if (!fullPath.endsWith("/")) {
        fullPath += "/";
    }
    fullPath += finalFileName;

    // 确保目标目录存在
    QDir dir;
    if (!dir.exists(filePath)) {
        dir.mkpath(filePath);
    }

    // 保存图像
    cv::imwrite(fullPath.toStdString(), img);
}


int HIKCamera::EnumDevices(QMap<QString, MV_CC_DEVICE_INFO*> &ds){
    int ret = Camera::EnumDevices(ds);
    devices = ds;
    return ret;
}

int HIKCamera::openDevice(QString name){
    camera = new Camera;
    int ret = camera->Open(devices[name]);
    if(ret == MV_OK){
        qDebug()<<"打开成功";
        ui->connect->setText("close");
        int ret = camera->StartGrabbing();
        if(ret==MV_OK){
            startDetect(true);
            ui->GetParam->click();
        }else{
            QMessageBox::information(this,"异常","采集设备失败");
        }
    }else QMessageBox::information(this,"异常","打开设备失败");

    return ret;
}

int HIKCamera::closeDevice(){
    startDetect(false);
    camera->StopGrabbing();
    camera->Close();
    qDebug()<<"断开成功";
    return 0;
}

bool HIKCamera::isOpened(){
    if(camera==MV_NULL) return false;

    return camera->isOpened();
}

int HIKCamera::startDIC(QString fileName,QString filePath,int duration){
    if(camera==MV_NULL) return -1;

    DICCounts = 0;
    DICFileName = fileName;
    DICFilePath = filePath;

    DICDetectTimer->start(duration);
    return 0;
}

int HIKCamera::stopDIC(){
    DICDetectTimer->stop();
    return 0;
}

void HIKCamera::setCurrentZreo(){
    A_to_B_Zreo = current_A_to_B;
}

void HIKCamera::setCamera(Camera &c){
    camera = &c;
}

void HIKCamera::startDetect(bool detect) {
    if (detect) {
        DetectTimer->start(200);
        detailsTimer->start(1000);
    }
    else {
        DetectTimer->stop();
        detailsTimer->stop();
    }
}

void drawRectangle(cv::Mat& image, const cv::Rect& rect, const cv::Scalar& color, int thickness = 1) {
    // 检查图像是否为空
    if(image.empty()) {
        std::cerr << "Image is empty!" << std::endl;
        return;
    }

    // 绘制矩形框
    cv::rectangle(image, rect, color, thickness);
}

void HIKCamera::catchOneFrameAndShow() {

    float m = 99;
    camera->GetTriggerMode(m);
    if (m == 1)startDetect(false);

    cv::Mat lastFrame = currentFrame.clone();


    int ret = camera->getOneImgMat(&currentFrame,10);
    if (ret != MV_OK)return;

    // 追踪器
    cv::Mat cloneFrame = currentFrame.clone();
    if (trackerWorkFlagA){
        extensometerA->caculateImgAsync(currentFrame);
        drawRectangle(cloneFrame,extensometerA->ROI, cv::Scalar(0, 0, 255),5);
    }
    if(trackerWorkFlagB) {
        extensometerB->caculateImgAsync(currentFrame);
        drawRectangle(cloneFrame,extensometerB->ROI, cv::Scalar(0, 255, 0),5);
    }


    cv::Mat rgbImg;
    if (cloneFrame.channels() == 3) {
        cvtColor(cloneFrame, rgbImg, cv::COLOR_BGR2RGB);
    }
    else {
        cloneFrame.copyTo(rgbImg);
    }

    if(boxEnable){
        if (boxRect.size() == 4) {
            std::vector<cv::Point> points;
            for (const auto& pt : boxRect) {
                points.push_back(cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)));
            }
            cv::polylines(rgbImg, points, true, cv::Scalar(0, 255, 255), 3);
        }

        if(boxPosition.first!=boxPosition.second){
            cv::Point startPoint(static_cast<int>(boxPosition.first.x), static_cast<int>(boxPosition.first.y));
            cv::Point endPoint(static_cast<int>(boxPosition.second.x), static_cast<int>(boxPosition.second.y));
            cv::line(rgbImg, startPoint, endPoint, cv::Scalar(0, 255, 0), 3);
        }
    }

    QImage qimg;
    if (rgbImg.channels() == 1)
        qimg = QImage(rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step, QImage::Format_Grayscale8);
    else if (rgbImg.channels() == 3)
        qimg = QImage(rgbImg.data, rgbImg.cols, rgbImg.rows, rgbImg.step, QImage::Format_RGB888);
    else
        return;
    qimg = qimg.copy();

    QPixmap pixmap = QPixmap::fromImage(qimg);
    viewScene->clear();
    QGraphicsPixmapItem* item = new QGraphicsPixmapItem(pixmap);
    viewScene->addItem(item);
    ui->graphicsView->fitInView(item, Qt::KeepAspectRatio);


}

void HIKCamera::DICSavePIC(){
    DICCounts ++;
    QtConcurrent::run([=]() {
        Mat img = currentFrame.clone();
        this->saveImageWithTimestamp(DICFileName, DICFilePath, img, DICCounts);
    });
}




void HIKCamera::getCameraDetails() {
    int framerate = 0;
    camera->GetFrameRate(framerate);
    ui->state_framerate->setText(QString::number(framerate));
}


void HIKCamera::enableControlWidget(bool enable){
    if(enable)ui->ControlWidget->show();
    else ui->ControlWidget->hide();
}

void HIKCamera::enableStateWidget(bool enable){
    if (enable) {
        ui->stateWidget->show();
    }
    else{
        ui->stateWidget->hide();
    }
}

void HIKCamera::enableConnectWidget(bool enable){
    if(enable)ui->connectWidget->show();
    else ui->connectWidget->hide();
}



void HIKCamera::on_GetParam_clicked()
{
    if(camera==MV_NULL)return;

    //曝光
    float value;
    camera->GetExposureTime(value);
    if(value==-2)ui->enableExposure->setCurrentIndex(2);
    else if (value == -1)ui->enableGain->setCurrentIndex(1);
    else
    {   ui->enableExposure->setCurrentIndex(0);
        ui->exposureRate->setValue(value);
    }


    //增益
    camera->GetGain(value);
    if (value == -1)ui->enableGain->setCurrentIndex(1);
    else if (value == -2)ui->enableGain->setCurrentIndex(2);
    else
    {
        ui->enableGain->setCurrentIndex(0);
        ui->gainRate->setValue(value);
    }


    camera->getDigitalGain(&value);
    if (value == -1)ui->enableDigitalGain->setCurrentIndex(0);
    else
    {
        ui->enableDigitalGain->setCurrentIndex(1);
        ui->digitalGainRate->setValue(value);
    }
    // qDebug() << "getDigitalGain" << value;



    int fr=-99;
    camera->GetFrameRate(fr);
    ui->frameRate->setValue(fr);
    // qDebug() << "getFrameRate" << fr;

    camera->getBanlenceWhite(&value);
    if (value == -1)ui->enableBanlenceWhite->setCurrentIndex(1);
    else if (value == -2)ui->enableBanlenceWhite->setCurrentIndex(2);
    else
    {
        ui->enableBanlenceWhite->setCurrentIndex(0);
    }
    // qDebug() << "getBanlenceWhite" << value;

    camera->getGamma(&value);
    if (value == -1)ui->enableGamma->setCurrentIndex(1);
    else
    {
        ui->enableGamma->setCurrentIndex(0);
        ui->gammaRate->setValue(value);
    }
    // qDebug() << "getGamma" << value;

    camera->getHeight(&value);
    ui->imgHight->setValue(value);

    camera->getWidth(&value);
    ui->imgWidth->setValue(value);

    camera->getMaxHeight(&value);
    ui->maxH->setText(QString::number(value));

    camera->getMaxWidth(&value);
    ui->maxW->setText(QString::number(value));



    bool exy = false;
    camera->getXReverse(&exy);
    ui->X_reverse->setChecked(exy);

    camera->getYReverse(&exy);
    ui->Y_reverse->setChecked(exy);
}

void HIKCamera::setParamsClicked() {
    if(camera==MV_NULL)return;

    //设置曝光
    if(ui->enableExposure->currentIndex()==0){
        camera->SetExposureTime(ui->exposureRate->value());
    }else if(ui->enableExposure->currentIndex()==1){
        camera->SetExposureTime(-1);
    }else if(ui->enableExposure->currentIndex()==2){
        camera->SetExposureTime(-2);
    }

    //设置增益
    if(ui->enableGain->currentIndex()==0){
        camera->SetGain(ui->gainRate->value());
    }else if(ui->enableGain->currentIndex()==1){
        camera->SetGain(-1);
    }else if(ui->enableGain->currentIndex()==2){
        camera->SetGain(-2);
    }

    //数字增益
    if(ui->enableDigitalGain->currentIndex()==0){
        camera->SetDigitalGain(-1);
    }else if(ui->enableDigitalGain->currentIndex()==1){
        camera->SetDigitalGain(ui->digitalGainRate->value());
    }

    //帧率控制
    if(ui->frameRate->value()==0){

    }else{

    }

    //白平衡
    if(ui->enableBanlenceWhite->currentIndex()==0){
        camera->setBanlenceWhite(ui->BanlenceWhite->value());
    }else{
        camera->setBanlenceWhite(-2);
    }

    //伽马校正
    if(ui->enableGamma->currentIndex()==0){
        camera->setGamma(-1);
    }else{
        camera->setGamma(ui->gammaRate->value());
    }

    //水平翻转
    camera->setXReverse(ui->X_reverse->isChecked());

    //垂直翻转 以及长宽
    camera->StopGrabbing();

    camera->setHeight(ui->imgHight->value());
    camera->setWidth(ui->imgWidth->value());

    camera->setYReverse(ui->Y_reverse->isChecked());

    camera->StartGrabbing();
}

Point2f nearestPointOnLine(QPair<Point2f, Point2f> position, Point2f point) {
    Point2f p1 = position.first;
    Point2f p2 = position.second;

    Point2f v = p2 - p1;            // 直线方向向量
    Point2f w = point - p1;         // 从 p1 到 point 的向量

    double dot_v_w = v.dot(w);      // 向量点积 v · w
    double dot_v_v = v.dot(v);      // v · v (即 |v|²)

    // 防止除以零（两点重合）
    if (dot_v_v == 0.0) {
        qDebug() << "Warning: Line points are identical.";
        return p1;
    }

    double t = dot_v_w / dot_v_v;   // 参数 t

    // 垂足坐标
    Point2f projection = p1 + t * v;

    // 计算点到直线的距离
    double distance = norm(point - projection);

    // 使用 qDebug 打印距离
    // qDebug() << "Distance from point to line:" << distance;

    return projection;
}

void HIKCamera::refreshPointA(float x, float y){

    A=nearestPointOnLine(boxPosition,Point2f(x,y));

    current_A_to_B = (current_A_to_B+norm(A-B))/DistenceRate;

    A_to_B_Distense = current_A_to_B - A_to_B_Zreo;
}

void HIKCamera::refreshPointB(float x, float y){
    B=nearestPointOnLine(boxPosition,Point2f(x,y));

    current_A_to_B = (current_A_to_B+norm(A-B))/DistenceRate;

    A_to_B_Distense = current_A_to_B - A_to_B_Zreo;
}

void HIKCamera::addTracker(QString ID) {
    if(camera==MV_NULL)return;

    cv::Mat img;
    int ret = camera->getOneImgMat(&img, 500);
    if (ret == MV_OK) {
        QPoint windowPosition = this->geometry().topLeft();
        QScreen* screen = QGuiApplication::screenAt(windowPosition);
        int screenWidth = 1920;
        int screenHeight = 1080;
        if (screen) {
            QRect screenGeometry = screen->geometry();
            screenWidth = screenGeometry.width();
            screenHeight = screenGeometry.height();
        }

        cv::Mat* resizedImage = new cv::Mat;
        Size originalSize = img.size();

        if (originalSize.width > originalSize.height) {
            cv::resize(img, *resizedImage, Size(screenWidth/2, screenWidth / 2 * originalSize.height / originalSize.width));
        }
        else {
            cv::resize(img, *resizedImage, Size(screenHeight / 2 * originalSize.width / originalSize.height, screenHeight / 2));
        }
        Size resizedSize = resizedImage->size();

        namedWindow("Select ROI", cv::WINDOW_AUTOSIZE);
        Rect roi_box = cv::selectROI("Select ROI", *resizedImage, false);
        if (roi_box.width == 0 || roi_box.height == 0) {
            emit throwError("No Roi Area Selected");
            return;
        }
        else {
            float scaleX = (float)originalSize.width / resizedSize.width;
            float scaleY = (float)originalSize.height / resizedSize.height;

            int originalX = static_cast<int>(roi_box.x * scaleX);
            int originalY = static_cast<int>(roi_box.y * scaleY);
            int originalWidth = static_cast<int>(roi_box.width * scaleX);
            int originalHeight = static_cast<int>(roi_box.height * scaleY);

            roi_box = { originalX, originalY, originalWidth, originalHeight };
        }

        destroyWindow("Select ROI");

        img = img.clone();

        if(ID=="A"){
            startTracker(false,trackerWorkFlagB);

            extensometerA->setOrignalImg(img);
            extensometerA->setROI(roi_box);

            extensometerA->uninit();
            extensometerA->init();

            extensometerA->debug(true);

            emit HIKEAROI(roi_box);
        }else if(ID=="B"){
            startTracker(trackerWorkFlagB,false);

            extensometerB->setOrignalImg(img);
            extensometerB->setROI(roi_box);

            extensometerB->uninit();
            extensometerB->init();

            extensometerB->debug(false);

            emit HIKEBROI(roi_box);
        }else return;
    }
}



double onMouse_length = 0;
bool onMouse_drawing = false;
cv::Point ptStart(-1, -1), ptEnd(-1, -1);
cv::Point mousePosition(-1, -1); // 用于存储当前鼠标位置

void onMouseAction(int event, int x, int y, int flags, void*) {
    if (event == cv::EVENT_LBUTTONDOWN) { // 左键按下
        ptStart = cv::Point(x, y);
        onMouse_drawing = true;
        mousePosition = cv::Point(x, y);
    }
    else if (event == cv::EVENT_MOUSEMOVE) { // 鼠标移动
        mousePosition = cv::Point(x, y);
        if (onMouse_drawing) {
            ptEnd = cv::Point(x, y);
        }
    }
    else if (event == cv::EVENT_LBUTTONUP) { // 左键释放
        ptEnd = cv::Point(x, y);
        onMouse_drawing = false;
        // 计算直线长度
        onMouse_length = std::sqrt(std::pow(ptEnd.x - ptStart.x, 2) + std::pow(ptEnd.y - ptStart.y, 2));

    }
}

void HIKCamera::selectLine(std::string windowtitle, Mat img) {

    QPoint windowPosition = this->geometry().topLeft();
    QScreen* screen = QGuiApplication::screenAt(windowPosition);
    int screenWidth = 1920;
    int screenHeight = 1080;
    if (screen) {
        QRect screenGeometry = screen->geometry();
        screenWidth = screenGeometry.width();
        screenHeight = screenGeometry.height();
    }

    cv::Mat* resizedImage = new cv::Mat;
    Size originalSize = img.size();

    if (originalSize.width > originalSize.height) {
        cv::resize(img, *resizedImage, Size(screenWidth / 2, screenWidth / 2 * originalSize.height / originalSize.width));
    }
    else {
        cv::resize(img, *resizedImage, Size(screenHeight / 2 * originalSize.width / originalSize.height, screenHeight / 2));
    }
    Size resizedSize = resizedImage->size();


    img = *resizedImage;
    cv::namedWindow(windowtitle);
    cv::setMouseCallback(windowtitle, onMouseAction, 0);


    int i = 0;

    while (true) {
        cv::Mat imgCopy = img.clone();
        if (ptStart.x >= 0 && ptStart.y >= 0 && ptEnd.x >= 0 && ptEnd.y >= 0) {
            cv::line(imgCopy, ptStart, ptEnd, cv::Scalar(0, 0, 255), 1); // 绘制红色线条
        }

        // 根据鼠标位置绘制放大视图
        if(mousePosition.x > 0 && mousePosition.y > 0){
            int margin = 20;
            cv::Rect roi(cv::Point(mousePosition.x - margin, mousePosition.y - margin),
                         cv::Point(mousePosition.x + margin, mousePosition.y + margin));
            roi &= cv::Rect(0, 0, imgCopy.cols, imgCopy.rows);
            cv::Mat roiImg = imgCopy(roi);
            cv::Mat zoomedRoi;
            int scale = 5;
            cv::resize(roiImg, zoomedRoi, cv::Size(), scale, scale);


            int cx = zoomedRoi.cols / 2;
            int cy = zoomedRoi.rows / 2;
            int len = 10;
            int thickness = 1;

            cv::line(zoomedRoi, cv::Point(cx - len, cy), cv::Point(cx + len, cy), cv::Scalar(0, 255, 0), thickness);
            cv::line(zoomedRoi, cv::Point(cx, cy - len), cv::Point(cx, cy + len), cv::Scalar(0, 255, 0), thickness);

            cv::circle(zoomedRoi, cv::Point(cx, cy), 1, cv::Scalar(0, 0, 255), -1);


            cv::imshow("Zoomed View", zoomedRoi);

            if(i ==0)
            {
                cv::setWindowProperty("Zoomed View", cv::WND_PROP_TOPMOST, 1);
                i =2;
            }


        }

        cv::imshow(windowtitle, imgCopy);

        int key = cv::waitKey(10);
        if (key == 27) {
            destroyWindow(windowtitle);
            destroyWindow("Zoomed View");
            break;
        }
        else if (key == 13) {
            destroyWindow(windowtitle);
            destroyWindow("Zoomed View");
            float scale = (float)originalSize.width / resizedSize.width;
            lineLength = onMouse_length * scale;

            emit LineSeclected(lineLength);

            break;
        }
    }
}

void HIKCamera::selectLine(std::string windowtitle){
    if(camera==MV_NULL)return;

    cv::Mat img;
    int ret = camera->getOneImgMat(&img, 500);
    if(ret==MV_OK)
        selectLine(windowtitle, img);
}

// 鼠标画线绘制方向 created by Tang 2025/10/15 18:37
void HIKCamera::selectPositionLine(std::string windowtitle){
    if(camera==MV_NULL)return;

    cv::Mat img;
    int ret = camera->getOneImgMat(&img, 500);
    if(ret!=MV_OK)
        return;


    QPoint windowPosition = this->geometry().topLeft();
    QScreen* screen = QGuiApplication::screenAt(windowPosition);
    int screenWidth = 1920;
    int screenHeight = 1080;
    if (screen) {
        QRect screenGeometry = screen->geometry();
        screenWidth = screenGeometry.width();
        screenHeight = screenGeometry.height();
    }

    cv::Mat* resizedImage = new cv::Mat;
    Size originalSize = img.size();

    if (originalSize.width > originalSize.height) {
        cv::resize(img, *resizedImage, Size(screenWidth / 2, screenWidth / 2 * originalSize.height / originalSize.width));
    }
    else {
        cv::resize(img, *resizedImage, Size(screenHeight / 2 * originalSize.width / originalSize.height, screenHeight / 2));
    }
    Size resizedSize = resizedImage->size();

    float scaleX = (float)originalSize.width / resizedSize.width;
    float scaleY = (float)originalSize.height / resizedSize.height;


    img = *resizedImage;
    cv::namedWindow(windowtitle);
    cv::setMouseCallback(windowtitle, onMouseAction, 0);


    while (true) {
        cv::Mat imgCopy = img.clone();
        if (ptStart.x >= 0 && ptStart.y >= 0 && ptEnd.x >= 0 && ptEnd.y >= 0) {
            cv::line(imgCopy, ptStart, ptEnd, cv::Scalar(0, 0, 255), 1); // 绘制红色线条
        }

        cv::imshow(windowtitle, imgCopy);

        int key = cv::waitKey(10);
        if (key == 27) {
            destroyWindow(windowtitle);
            break;
        }
        else if (key == 13) {
            destroyWindow(windowtitle);
            Mat output = resizedImage->clone();
            line(output, ptStart, ptEnd, Scalar(0, 255, 0), 1);

            // 显示结果
            imshow("Fitted Rectangles", output);
            waitKey(0);
            destroyAllWindows();

            Point2f scaleNewMid1 = Point2f(ptStart.x*scaleX, ptStart.y*scaleY);
            Point2f scaleNewMid2 = Point2f(ptEnd.x*scaleX, ptEnd.y*scaleY);

            boxRect.clear();
            boxPosition = qMakePair(scaleNewMid1,scaleNewMid2);
            emit PositionSelected(scaleNewMid1, scaleNewMid2);

            break;
        }
    }
}

// 鼠标画框自动识别方向 created by Tang 2025/10/15 19:06
void HIKCamera::autoSelectPositionLine(std::string windowtitle){
    if(camera==MV_NULL)return;

    cv::Mat img;
    int ret = camera->getOneImgMat(&img, 500);
    if(ret!=MV_OK)
        return;

    QPoint windowPosition = this->geometry().topLeft();
    QScreen* screen = QGuiApplication::screenAt(windowPosition);
    int screenWidth = 1920;
    int screenHeight = 1080;
    if (screen) {
        QRect screenGeometry = screen->geometry();
        screenWidth = screenGeometry.width();
        screenHeight = screenGeometry.height();
    }

    cv::Mat* resizedImage = new cv::Mat;
    Size originalSize = img.size();

    if (originalSize.width > originalSize.height) {
        cv::resize(img, *resizedImage, Size(screenWidth/2, screenWidth / 2 * originalSize.height / originalSize.width));
    }
    else {
        cv::resize(img, *resizedImage, Size(screenHeight / 2 * originalSize.width / originalSize.height, screenHeight / 2));
    }

    Size resizedSize = resizedImage->size();
    float scaleX = (float)originalSize.width / resizedSize.width;
    float scaleY = (float)originalSize.height / resizedSize.height;

    namedWindow(windowtitle, cv::WINDOW_AUTOSIZE);
    Rect box = cv::selectROI(windowtitle, *resizedImage, false);
    destroyWindow(windowtitle);

    if (box.empty()) {
        qDebug() << "No ROI selected.";
        return;
    }

    // 提取 ROI
    Mat roi = (*resizedImage)(box).clone();

    // 转为灰度图
    Mat gray;
    cvtColor(roi, gray, COLOR_BGR2GRAY);

    // 二值化
    Mat binary;
    threshold(gray, binary, 0, 255, THRESH_BINARY | THRESH_OTSU);

    // 形态学操作去除噪声
    morphologyEx(binary, binary, MORPH_OPEN, getStructuringElement(MORPH_ELLIPSE, Size(3,3)));

    // 查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    std::vector<Vec4i> hierarchy;
    findContours(binary, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    // 没有轮廓退出
    if (contours.empty()) {
        qDebug() << "No contour found in ROI.";
        return;
    }
    // 找最大轮廓（假设是条状标距段）
    int maxIdx = 0;
    double maxArea = 0;
    for (int i = 0; i < contours.size(); i++) {
        double area = contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxIdx = i;
        }
    }

    // 获取最大轮廓
    std::vector<cv::Point> largestContour = contours[maxIdx];

    // 计算最小外接旋转矩形
    RotatedRect minRect = minAreaRect(largestContour);
    Point2f rectPoints[4];
    minRect.points(rectPoints);

    // 将旋转矩形的顶点从 ROI 坐标转换为显示图像坐标
    for(int i = 0; i < 4; ++i){
        rectPoints[i].x += box.x;
        rectPoints[i].y += box.y;
    }

    // 创建输出图像
    Mat output = resizedImage->clone();

    // 绘制识别到的矩形
    boxRect.clear();
    for(int i = 0; i < 4; ++i){
        line(output, rectPoints[i], rectPoints[(i+1)%4], Scalar(0, 255, 255), 1);
        boxRect.append(Point2f(rectPoints[i].x*scaleX, rectPoints[i].y*scaleY));
    }

    // 计算四条边的长度，找出长边
    double len01 = norm(rectPoints[1] - rectPoints[0]);
    double len12 = norm(rectPoints[2] - rectPoints[1]);

    Point2f mid1, mid2;
    if (len01 < len12) {
        // 边 0-1 和 2-3 是长边
        mid1 = (rectPoints[0] + rectPoints[1]) * 0.5f;
        mid2 = (rectPoints[2] + rectPoints[3]) * 0.5f;
    } else {
        // 边 1-2 和 3-0 是长边
        mid1 = (rectPoints[1] + rectPoints[2]) * 0.5f;
        mid2 = (rectPoints[3] + rectPoints[0]) * 0.5f;
    }

    // 计算中线的方向向量
    Point2f direction = mid2 - mid1;
    direction = direction / norm(direction);// 归一化

    // 延长中线至两倍长度
    Point2f newMid1 = mid1 - direction * norm(mid2 - mid1);
    Point2f newMid2 = mid2 + direction * norm(mid2 - mid1);

    Point2f scaleNewMid1 = Point2f(newMid1.x*scaleX, newMid1.y*scaleY);
    Point2f scaleNewMid2 = Point2f(newMid2.x*scaleX, newMid2.y*scaleY);
    boxPosition = qMakePair(scaleNewMid1,scaleNewMid2);

    emit PositionSelected(scaleNewMid1, scaleNewMid2);
}

// 自动选择标记点
void HIKCamera::autoAddTracker(std::string windowtitle){

}

// 棋盘格畸变校正
