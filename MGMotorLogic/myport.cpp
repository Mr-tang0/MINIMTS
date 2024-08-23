#include "myport.h"

#include <QThread>
#include <QDebug>
myPort::myPort(QObject *parent) :
    QObject(parent)
{

    QThread *thread = new QThread;

    decoder->moveToThread(thread);
    thread->start();

    connect(serialPort,&QSerialPort::readyRead,this,[=](){
        QByteArray data = serialPort->readAll();
        decoder->decode(data);
    });
//    connect(decoder,&decodeThread::decodeDone,this,[=](QMap<QString,QString> decodeMap){
//        emit decodeDone(decodeMap);
//    });
    connect(sendTimer,&QTimer::timeout,this,[=](){
        if(buffer.empty())return;
        sendMessage(buffer.first());
        buffer.removeFirst();
    });

}

myPort::~myPort()
{
    if (serialPort && serialPort->isOpen()) {
        serialPort->close();
    }
    delete serialPort;
    sendTimer->stop();
    delete sendTimer;
}

QStringList myPort::findPort(){
    const QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();
    QStringList portNames;
    for (const QSerialPortInfo &portInfo : availablePorts) {
        portNames.append(portInfo.portName());
    }
    return portNames;
}

bool myPort::connectPort(const QString &portName, int baudRate){
    if (serialPort && serialPort->isOpen()) {
        return false;
    }

    serialPort->setPortName(portName);
    serialPort->setBaudRate(baudRate);
    if (serialPort->open(QIODevice::ReadWrite)) {
        buffer.clear();
        sendTimer->start(50);
        return true;
    } else {
        return false;
    }
}

void myPort::disConnectPort(){
    if (serialPort && serialPort->isOpen()) {
        serialPort->close();
        buffer.clear();
    }
    sendTimer->stop();
}

void myPort::getCurrentValue(float *angle,float *length,float *force,float *speed){
    *angle = decoder->currentMotorLength;
    *length = decoder->currentFactLength;
    *force = decoder->currentForceValue;
    *speed = decoder->currentSpeedValue;
}

void myPort::appendBuffer(QString data){
    const QStringList hexNumbers = data.split(' ');

    QByteArray byteArray;
    for (const QString &hexNumber : hexNumbers) {
        quint8 byte = hexNumber.toUInt(nullptr, 16);
        byteArray.append(byte);
    }

    buffer.append(byteArray);
}

void myPort::prependBuffer(QString data){
    const QStringList hexNumbers = data.split(' ');

    QByteArray byteArray;
    for (const QString &hexNumber : hexNumbers) {
        quint8 byte = hexNumber.toUInt(nullptr, 16);
        byteArray.append(byte);
    }

    buffer.prepend(byteArray);
}


bool myPort::sendMessage(QByteArray data){
    if (serialPort && serialPort->isOpen()) {
        qint64 bytesWritten = serialPort->write(data);
        if (bytesWritten == data.size()) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}
