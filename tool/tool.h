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

    static bool saveCsvData(const QList<QList<float>> &data, const QList<QString> &header, QString filePath);
    static bool saveCsvData(const QList<QList<float>> &data, const QList<QString> &header, QMap<QString,QString>datails, QString filePath);
    static QList<QList<float>> loadCsvData(QString filePath, QList<QString> &header);

    static void dataSmooth(QList<float> &data,QString type,int window);
    static void interpolation(QList<float> &data,QString type,int counts);
    static std::vector<float> solveTridiagonalSystem(const std::vector<std::vector<float>> &A, const std::vector<float> &b);

    static float max(QList<float> list) {
        QList<float>::iterator maxIter = std::max_element(list.begin(), list.end());
        return *maxIter;
    }

    static float min(QList<float> list) {
        QList<float>::iterator minIter = std::min_element(list.begin(), list.end());
        return *minIter;
    }

    static float median(QList<float> list) {
        std::sort(list.begin(), list.end());
        int size = list.size();
        int midIndex = size / 2;

        if (size % 2 == 0)
            return (list[midIndex - 1] + list[midIndex]) / 2.0f;
        else
            return list[midIndex];
    }

    static bool removeDirectoryRecursively(const QString &path);
    static void copyPathToClipboard(const QString &path);


private:

};

#endif // TOOL_H
