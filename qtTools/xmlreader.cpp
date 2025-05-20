#include "xmlreader.h"

XMLReader::XMLReader(QObject *parent)
    : QObject{parent}
{}

QString XMLReader::hello(){
    return "hello";
}

bool XMLReader::Save_XML_File(QMap<QString,QMap<QString,QString>>data,QString path){
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << dir.absolutePath();
            return false;
        }
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for writing:" << path;
        return false;
    }

    QDomDocument doc;

    QDomElement root = doc.createElement("root");
    doc.appendChild(root);

    for (auto it1 = data.constBegin(); it1 != data.constEnd(); ++it1) {

        QDomElement motorElement = doc.createElement(it1.key());
        root.appendChild(motorElement);

        for (auto it2 = it1.value().constBegin(); it2 != it1.value().constEnd(); ++it2) {
            QDomElement propertyElement = doc.createElement(it2.key());
            propertyElement.appendChild(doc.createTextNode(it2.value()));
            motorElement.appendChild(propertyElement);
        }
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    doc.save(out, 4);
    file.close();

    return true;
}

QMap<QString,QMap<QString,QString>> XMLReader::Read_XML_File(QString path){
    QMap<QString, QMap<QString, QString>> data;

    if (!QFile::exists(path)) {
        qWarning() << "File does not exist:" << path;
        Save_XML_File(data,path);
        return data;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for reading:" << path;
        return data;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qWarning() << "Error parsing XML file:" << path;
        file.close();
        return data;
    }
    file.close();

    QDomElement root = doc.documentElement();
    if (root.tagName() != "root") {
        qWarning() << "Unexpected root element. Expected 'root' but found:" << root.tagName();
        return data;
    }

    QDomNode node = root.firstChild();
    while (!node.isNull()) {
        if (node.isElement()) {
            QDomElement element = node.toElement();
            QString motorName = element.tagName();
            QMap<QString, QString> properties;

            QDomNode propertyNode = element.firstChild();
            while (!propertyNode.isNull()) {
                if (propertyNode.isElement()) {
                    QDomElement propertyElement = propertyNode.toElement();
                    QString propertyName = propertyElement.tagName();
                    QString propertyValue = propertyElement.text();
                    properties.insert(propertyName, propertyValue);
                }
                propertyNode = propertyNode.nextSibling();
            }

            data.insert(motorName, properties);
        }
        node = node.nextSibling();
    }

    return data;
}
