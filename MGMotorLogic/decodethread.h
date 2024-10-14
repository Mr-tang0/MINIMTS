#ifndef DECODETHREAD_H
#define DECODETHREAD_H

#include <QObject>
#include <QMap>

class decodeThread : public QObject
{
    Q_OBJECT
public:
    explicit decodeThread(QObject *parent = nullptr);
    float currentMotorLength = 0;
    float currentFactLength = 0;
    float currentForceValue = 0;
    float currentSpeedValue = 0;

    float QByteArrayTofloat(QByteArray data, QString utils);

public slots:
    void decode(QByteArray data);

private:
   QByteArray reseiveBuffer;


signals:
    void decodeLengthValue(int value);
    void decodeForceValue(int value);
    void decodeFactLengthValue(int value);
    void decodeSpeedValue(int value);
    void decodeLimitOccured(int ID, int axis);

    void testOK();
    void motorDetails(QString m);


};

#endif // DECODETHREAD_H
