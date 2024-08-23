#ifndef TOOL_H
#define TOOL_H

#include "tool_global.h"
#include <QtCore/QtCore>
#include <QMap>

struct User
{
    QString userid = "0";

    QString name = "default";

    QString password = "";

    QString role = " ";

    QString userDefaultName = "data";

    QString userfilePath = "";

    QString lastTestTime = "";

    QString headerPic = "";

};

class TOOL_EXPORT Tool
{
public:
    Tool();

    static QMap<QString, QMap<QString, QString>> readXMlFile(QString filePath);
    static void writeXMlFile(QString filePath, QMap<QString, QMap<QString, QString>> map);

    static QString numberVerify(QString string);
    static QString speedToQstring(double);
    static QString lengthToQstring(double);
    static QString crc16Ccitt(QString string);


    static bool addUser(User user,QMap<QString, QMap<QString, QString>>*);
    static User getUser(QString userName,QString password,QMap<QString, QMap<QString, QString>>);
    static User getUser(QString id,QMap<QString, QMap<QString, QString>>);

    static bool freshUser(User user,QMap<QString, QMap<QString, QString>>*);
    static bool deleteUser(User user,QMap<QString, QMap<QString, QString>>*);

    static void delay(unsigned int msec)
    {
        QEventLoop loop;
        QTimer::singleShot(msec, &loop, SLOT(quit()));
        loop.exec();
    }

    static void saveCsvData(const QList<QList<float>> &data, const QList<QString> &header, QString filePath);
    static QList<QList<float>> loadCsvData(QString filePath, QList<QString> &header);

private:

};

#endif // TOOL_H
