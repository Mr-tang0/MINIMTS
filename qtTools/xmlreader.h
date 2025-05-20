#ifndef XMLREADER_H
#define XMLREADER_H

#include <QObject>
#include <QMap>
#include <QFile>
#include <QDir>
#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>
#include <QIODevice>
#include <QDebug>

class XMLReader : public QObject
{
    Q_OBJECT
public:
    explicit XMLReader(QObject *parent = nullptr);

    static QString hello();
    static bool Save_XML_File(QMap<QString,QMap<QString,QString>>data,QString path);
    static QMap<QString,QMap<QString,QString>> Read_XML_File(QString path);

signals:
};

#endif // XMLREADER_H
