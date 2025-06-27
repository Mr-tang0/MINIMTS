#include "hikcamera.h"
#include "ui_hikcamera.h"
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QtConcurrent>
#include <QScreen>




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




    connect(DetectTimer, &QTimer::timeout, this, &HIKCamera::catchOneFrameAndShow);
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

    connect(&HIKEA,&HIKExtensometer::resultEmit,this,&HIKCamera::refreshPointA);
    connect(&HIKEB,&HIKExtensometer::resultEmit,this,&HIKCamera::refreshPointB);

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
    DICEnable = true;

    startDetect(false);
    DetectTimer->start(duration);
    return 0;
}

int HIKCamera::stopDIC(){
    DICEnable = false;

    startDetect(true);
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

/**
 * 在给定的图像上绘制一个矩形框。
 *
 * @param image 输入输出图像，矩形框将被绘制在此图像上。
 * @param rect 要绘制的矩形框的位置和大小。
 * @param color 矩形边框的颜色，以Scalar对象表示 (B, G, R)。
 * @param thickness 矩形边框的粗细。如果为负值，如 FILLED，则填充矩形。
 */
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

    Mat currentFrame;
    int ret = camera->getOneImgMat(&currentFrame,10);
    if (ret != MV_OK)return;

    //追踪器
    if (trackerWorkFlagA){
        HIKEA.caculateImgAsync(currentFrame);
        drawRectangle(currentFrame,HIKEA.ROI, cv::Scalar(0, 0, 255),2);
    }
    if(trackerWorkFlagB) {
        HIKEB.caculateImgAsync(currentFrame);
        drawRectangle(currentFrame,HIKEB.ROI, cv::Scalar(0, 255, 0),2);
    }

    cv::Mat rgbImg;
    if (currentFrame.channels() == 3) {
        cvtColor(currentFrame, rgbImg, cv::COLOR_BGR2RGB);
    }
    else {
        currentFrame.copyTo(rgbImg);
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

    if(DICEnable)
    {
        DICCounts ++;
        QtConcurrent::run([=]() {
            this->saveImageWithTimestamp(DICFileName, DICFilePath, currentFrame, DICCounts);
        });
    }
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

void HIKCamera::refreshPointA(float x, float y){
    A.x = x;
    A.y = y;
    current_A_to_B = (current_A_to_B+std::sqrt(std::pow(B.x - A.x, 2) + std::pow(B.y - A.y, 2)) / DistenceRate)/2;
    current_A_to_B = int(current_A_to_B*1000)/1000.0;

    A_to_B_Distense = current_A_to_B - A_to_B_Zreo;
}

void HIKCamera::refreshPointB(float x, float y){
    B.x = x;
    B.y = y;
    current_A_to_B = (current_A_to_B+std::sqrt(std::pow(B.x - A.x, 2) + std::pow(B.y - A.y, 2)) / DistenceRate)/2;
    current_A_to_B = int(current_A_to_B*1000)/1000.0;

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
            HIKEA.uninit();
            HIKEA.setOrignalImg(img);
            HIKEA.setROI(roi_box);
            HIKEA.init();
            HIKEA.debug(true);
            emit HIKEAROI(roi_box);
        }else if(ID=="B"){
            startTracker(trackerWorkFlagB,false);
            HIKEB.uninit();
            HIKEB.setOrignalImg(img);
            HIKEB.setROI(roi_box);
            HIKEB.init();
            HIKEB.debug(false);
            emit HIKEBROI(roi_box);
        } else return;
    }
}



double onMouse_length = 0;
bool onMouse_drawing = false;
cv::Point ptStart(-1, -1), ptEnd(-1, -1);
void onMouse(int event, int x, int y, int flags, void*) {
    if (event == cv::EVENT_LBUTTONDOWN) { // 左键按下
        ptStart = cv::Point(x, y);
        onMouse_drawing = true;
    }
    else if (event == cv::EVENT_MOUSEMOVE) { // 鼠标移动
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
    cv::setMouseCallback(windowtitle, onMouse, 0);

    while (true) {
        cv::Mat imgCopy = img.clone();
        if (ptStart.x >= 0 && ptStart.y >= 0 && ptEnd.x >= 0 && ptEnd.y >= 0) {
            cv::line(imgCopy, ptStart, ptEnd, cv::Scalar(0, 0, 255), 2); // 绘制红色线条
        }
        cv::imshow(windowtitle, imgCopy);

        int key = cv::waitKey(10);
        if (key == 27) {
            destroyWindow(windowtitle);
            break;
        }
        else if (key == 13) {
            destroyWindow(windowtitle);
            float scale = (float)originalSize.width / resizedSize.width;
            lineLength = onMouse_length * scale;
            qDebug() << "lineLength" << lineLength;
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
