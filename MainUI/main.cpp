#include "mainwindow.h"
#include "appinit.h"
#include "quihelper.h"
#include <QApplication>
#include <QProcess>
#include <opencv2/opencv.hpp>
#include <QMetaType>



int main(int argc, char *argv[])
{

    qRegisterMetaType<cv::Mat>("cv::Mat");

    QUIHelper::initMain();
    QApplication a(argc, argv);
    AppInit::Instance()->start();

    QUIHelper::setFont();
    QUIHelper::setCode();

    //加载样式表
    QFile file(":/qss/res/core_qss/qss/blacksoft.css");
    if (file.open(QFile::ReadOnly)) {
        QString qss = QLatin1String(file.readAll());
        QString paletteColor = qss.mid(20, 7);
        qApp->setPalette(QPalette(QColor(paletteColor)));
        qApp->setStyleSheet(qss);
        file.close();
    }



    MainWindow w;

    w.resize(1200, 800);
    QUIHelper::setFormInCenter(&w);
    // w.show();

    return a.exec();
}
