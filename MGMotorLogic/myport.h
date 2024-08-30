#ifndef MYPORT_H
#define MYPORT_H

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QObject>
#include <QMap>
#include <QTimer>
#include "decodethread.h"
#include <QTime>
#include <QCoreApplication>

class myPort:public QObject
{
    Q_OBJECT

public:
    explicit myPort(QObject *parent = nullptr);
    ~myPort();

    QStringList findPort();
    bool connectPort(const QString &portName, int buandRate);
    void disConnectPort();
    void appendBuffer(QString data);
    void prependBuffer(QString data);

    void getCurrentValue(float *angle,float *length,float *force,float *speed);

    decodeThread *decoder = new decodeThread;

private:
    QSerialPort *serialPort = new QSerialPort;
    QList<QByteArray> buffer;
    bool sendMessage(QByteArray data);
    QTimer *sendTimer = new QTimer(this);


    void delay(unsigned int msec)
    {
        QEventLoop loop;//定义一个新的事件循环
        QTimer::singleShot(msec, &loop, SLOT(quit()));//创建单次定时器，槽函数为事件循环的退出函数
        loop.exec();//事件循环开始执行，程序会卡在这里，直到定时时间到，本循环被退出
    }

signals:
    void decodeDone(QMap<QString,QString>);

};

#endif // MYPORT_H
