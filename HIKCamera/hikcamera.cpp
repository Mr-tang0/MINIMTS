#include "hikcamera.h"
#include "ui_hikcamera.h"
#include <QDebug>
#include <QMessageBox>
#include <QDir>
#include <QtConcurrent>




HIKCamera::HIKCamera(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HIKCamera)
{
    ui->setupUi(this);

    camera = MV_NULL;

    enableControlWidget(false);
    enableStateWidget(false);
    enableConnectWidget(false);


    connect(DetectTimer, &QTimer::timeout, this, &HIKCamera::catchOneFrameAndShow);
    connect(detailsTimer, &QTimer::timeout, this, &HIKCamera::getCameraDetails);

    connect(ui->SetParam, &QPushButton::clicked, this, &HIKCamera::setParamsClicked);

    connect(ui->close, &QPushButton::clicked, this, [=]() {enableControlWidget(false); });

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

void HIKCamera::setCamera(Camera &c){
    camera = &c;
}

void HIKCamera::startDetect(bool detect) {
    if (detect) {
        DetectTimer->start(100);
        detailsTimer->start(1000);
    }
    else {
        DetectTimer->stop();
        detailsTimer->stop();
    }
}


void HIKCamera::catchOneFrameAndShow() {

    float m = 99;
    camera->GetTriggerMode(m);
    if (m == 1)startDetect(false);

    Mat currentFrame;
    int ret = camera->getOneImgMat(&currentFrame,10);
    if (ret != MV_OK)return;


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

void HIKCamera::calibration_clicked() {
    cv::Mat img;
    // int ret = camera->getOneImgMat(&img, 500);
    // if (ret == MV_OK) {
    //     namedWindow("Select LINE", cv::WINDOW_AUTOSIZE);
    //     selectLine("Select LINE", img);
    //     emit lineSeclected(lineLength);
    // }
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
    float value;
    camera->GetExposureTime(value);
    if(value==-1)ui->enableExposure->setCurrentIndex(1);
    else
    {   ui->enableExposure->setCurrentIndex(0);
        ui->exposureRate->setValue(value);
    }
    qDebug()<<"enableExposure"<<value;

    camera->getDigitalGain(&value);
    if (value == -1)ui->enableDigitalGain->setCurrentIndex(1);
    else
    {
        ui->enableDigitalGain->setCurrentIndex(0);
        ui->digitalGainRate->setValue(value);
    }
    qDebug() << "getDigitalGain" << value;

    camera->GetGain(value);
    if (value == -1)ui->enableGain->setCurrentIndex(1);
    else if (value == -2)ui->enableGain->setCurrentIndex(2);
    else
    {
        ui->enableGain->setCurrentIndex(0);
        ui->gainRate->setValue(value);
    }
    qDebug() << "getGain" << value;

    int fr=-99;
    camera->GetFrameRate(fr);
    ui->frameRate->setValue(fr);
    qDebug() << "getFrameRate" << fr;

    camera->getBanlenceWhite(&value);
    if (value == -1)ui->enableBanlenceWhite->setCurrentIndex(1);
    else if (value == -2)ui->enableBanlenceWhite->setCurrentIndex(2);
    else
    {
        ui->enableBanlenceWhite->setCurrentIndex(0);
    }
    qDebug() << "getBanlenceWhite" << value;

    camera->getGamma(&value);
    if (value == -1)ui->enableGamma->setCurrentIndex(1);
    else
    {
        ui->enableGamma->setCurrentIndex(0);
        ui->gammaRate->setValue(value);
    }
    qDebug() << "getGamma" << value;

    camera->getHeight(&value);
    ui->imgHight->setValue(value);
    qDebug() << "getHeight" << value;

    camera->getWidth(&value);
    ui->imgWidth->setValue(value);
    qDebug() << "getWidth" << value;

    bool exy = false;
    camera->getXReverse(&exy);
    ui->X_reverse->setChecked(exy);

    camera->getYReverse(&exy);
    ui->Y_reverse->setChecked(exy);
}

void HIKCamera::setParamsClicked() {


}



// double onMouse_length = 0;
// bool onMouse_drawing = false;
// cv::Point ptStart(-1, -1), ptEnd(-1, -1);
// void onMouse(int event, int x, int y, int flags, void*) {
//     if (event == cv::EVENT_LBUTTONDOWN) { // 左键按下
//         ptStart = cv::Point(x, y);
//         onMouse_drawing = true;
//     }
//     else if (event == cv::EVENT_MOUSEMOVE) { // 鼠标移动
//         if (onMouse_drawing) {
//             ptEnd = cv::Point(x, y);
//         }
//     }
//     else if (event == cv::EVENT_LBUTTONUP) { // 左键释放
//         ptEnd = cv::Point(x, y);
//         onMouse_drawing = false;
//         // 计算直线长度
//         onMouse_length = std::sqrt(std::pow(ptEnd.x - ptStart.x, 2) + std::pow(ptEnd.y - ptStart.y, 2));

//     }
// }

// void HIKCamera::selectLine(std::string windowtitle, Mat img) {

//     QPoint windowPosition = this->geometry().topLeft();
//     QScreen* screen = QGuiApplication::screenAt(windowPosition);
//     int screenWidth = 1920;
//     int screenHeight = 1080;
//     if (screen) {
//         QRect screenGeometry = screen->geometry();
//         screenWidth = screenGeometry.width();
//         screenHeight = screenGeometry.height();
//     }

//     cv::Mat* resizedImage = new cv::Mat;
//     Size originalSize = img.size();

//     if (originalSize.width > originalSize.height) {
//         cv::resize(img, *resizedImage, Size(screenWidth / 2, screenWidth / 2 * originalSize.height / originalSize.width));
//     }
//     else {
//         cv::resize(img, *resizedImage, Size(screenHeight / 2 * originalSize.width / originalSize.height, screenHeight / 2));
//     }
//     Size resizedSize = resizedImage->size();


//     img = *resizedImage;
//     cv::namedWindow(windowtitle);
//     cv::setMouseCallback(windowtitle, onMouse, 0);

//     while (true) {
//         cv::Mat imgCopy = img.clone();
//         if (ptStart.x >= 0 && ptStart.y >= 0 && ptEnd.x >= 0 && ptEnd.y >= 0) {
//             cv::line(imgCopy, ptStart, ptEnd, cv::Scalar(0, 0, 255), 2); // 绘制红色线条
//         }
//         cv::imshow(windowtitle, imgCopy);

//         int key = cv::waitKey(10);
//         if (key == 27) {
//             destroyWindow(windowtitle);
//             break;
//         }
//         else if (key == 13) {
//             destroyWindow(windowtitle);
//             float scale = (float)originalSize.width / resizedSize.width;
//             lineLength = onMouse_length * scale;
//             qDebug() << "lineLength" << lineLength;
//             break;
//         }
//     }
// }
