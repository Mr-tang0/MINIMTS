#ifndef MINIMTSPORT_H
#define MINIMTSPORT_H

#include <QWidget>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QObject>
#include <QMap>
#include <QTimer>
#include "minimtsdecoder.h"


namespace Ui {
class MiniMTSPort;
}

class MiniMTSPort : public QWidget
{
    Q_OBJECT

public:
    explicit MiniMTSPort(QWidget *parent = nullptr);
    ~MiniMTSPort();

    QStringList findPort();
    bool connectPort(const QString &portName, int buandRate);
    void disConnectPort();
    bool sendMessage(QByteArray data);

    void appendBuffer(QString data);
    void prependBuffer(QString data);

    void appendBuffer(QByteArray data);
    void prependBuffer(QByteArray data);

    void clearBuffer();

    MiniMTSDecoder *decoder = new MiniMTSDecoder;

private slots:
    void on_refresh_clicked();

private:
    Ui::MiniMTSPort *ui;

    QList<QByteArray> buffer;
    QSerialPort *serialPort = new QSerialPort;
    QTimer *sendTimer = new QTimer(this);
};

#endif // MINIMTSPORT_H
