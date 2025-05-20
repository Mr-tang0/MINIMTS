#include "minimtsport.h"
#include "ui_minimtsport.h"
#include <QThread>

MiniMTSPort::MiniMTSPort(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MiniMTSPort)
{
    ui->setupUi(this);

    QThread *thread = new QThread;
    decoder->moveToThread(thread);

    connect(serialPort, &QSerialPort::readyRead, this, [this]() {
        decoder->decode(serialPort->readAll());
    });
    
    thread->start();

    connect(sendTimer,&QTimer::timeout,this,[this](){
        if(buffer.empty())return;
        sendMessage(buffer.first());
        buffer.removeFirst();
    });

}

MiniMTSPort::~MiniMTSPort()
{
    delete ui;
}


QStringList MiniMTSPort::findPort(){
    const QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();
    QStringList portNames;
    for (const QSerialPortInfo &portInfo : availablePorts) {
        portNames.append(portInfo.portName());
    }
    return portNames;
}

bool MiniMTSPort::connectPort(const QString &portName, int baudRate){
    if (serialPort && serialPort->isOpen()) {
        return false;
    }

    serialPort->setPortName(portName);
    serialPort->setBaudRate(baudRate);

    if (serialPort->open(QIODevice::ReadWrite)) {
        clearBuffer();
        sendTimer->start(20);
        return true;
    } else {
        return false;
    }
}

void MiniMTSPort::disConnectPort(){
    if (serialPort && serialPort->isOpen()) {
        serialPort->close();
        clearBuffer();
        sendTimer->stop();
    }
}

bool MiniMTSPort::sendMessage(QByteArray data){
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

void MiniMTSPort::prependBuffer(QString data){
    const QStringList hexNumbers = data.split(' ');

    QByteArray byteArray;
    for (const QString &hexNumber : hexNumbers) {
        quint8 byte = hexNumber.toUInt(nullptr, 16);
        byteArray.append(byte);
    }

    buffer.prepend(byteArray);
}


void MiniMTSPort::appendBuffer(QString data){
    const QStringList hexNumbers = data.split(' ');

    QByteArray byteArray;
    for (const QString &hexNumber : hexNumbers) {
        quint8 byte = hexNumber.toUInt(nullptr, 16);
        byteArray.append(byte);
    }

    buffer.append(byteArray);
}

void MiniMTSPort::appendBuffer(QByteArray data){
    buffer.append(data);
}

void MiniMTSPort::prependBuffer(QByteArray data){
    buffer.prepend(data);
}

void MiniMTSPort::clearBuffer(){
    buffer.clear();
}

void MiniMTSPort::on_refresh_clicked()
{
    // emit connect();
}

