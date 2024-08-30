#include "tool.h"
#include <QDebug>
#include <QFile>
#include <QDomDocument>
#include <QDomElement>
#include <QDomNode>
#include <algorithm>
#include <vector>
#include <cmath>
#include <QClipboard>
#include <QApplication>

Tool::Tool()
{

}

QMap<QString, QMap<QString, QString>> Tool::readXMlFile(QString filePath)
{
    // 创建一个QMap来存储结果
    QMap<QString, QMap<QString, QString>> resultMap;

    if (!QFile::exists(filePath)) {
        // 创建新文件
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "Failed to create file:" << filePath;
            return resultMap;
        }

        // 初始化文件内容
        QDomDocument doc;
        QDomProcessingInstruction pi = doc.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\"");
        doc.appendChild(pi);

        QDomElement root = doc.createElement("root");
        doc.appendChild(root);

        // 写入文件
        QTextStream out(&file);
        doc.save(out, 4); // 缩进为4个空格
        file.close();
    }

    // 加载并解析XML文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << filePath;
        return resultMap;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qDebug() << "Failed to parse XML file:" << filePath;
        file.close();
        return resultMap;
    }
    file.close();

    // 获取根节点
    QDomElement root = doc.documentElement();
    if (root.tagName() == "root") {
        // 遍历子节点
        QDomNode node = root.firstChild();
        while (!node.isNull()) {
            QDomElement element = node.toElement();
            if (!element.isNull()) {
                // 获取子节点的标签名作为外层QMap的键
                QString outerKey = element.tagName();

                // 创建一个新的QMap来存储次子节点的数据
                QMap<QString, QString> innerMap;

                // 遍历次子节点
                QDomNode innerNode = element.firstChild();
                while (!innerNode.isNull()) {
                    QDomElement innerElement = innerNode.toElement();
                    if (!innerElement.isNull()) {
                        // 获取次子节点的标签名作为内层QMap的键
                        QString innerKey = innerElement.tagName();
                        // 获取次子节点的文本内容作为内层QMap的值
                        QString innerValue = innerElement.text();
                        innerMap.insert(innerKey, innerValue);
                    }
                    innerNode = innerNode.nextSibling();
                }

                // 将内层QMap添加到外层QMap中
                resultMap.insert(outerKey, innerMap);
            }
            node = node.nextSibling();
        }
    } else {
        qDebug() << "Invalid root element";
    }
    file.close();

    return resultMap;
}

void Tool::writeXMlFile(QString filePath, QMap<QString, QMap<QString, QString>> map)
{
    if(!filePath.contains(":"))filePath = QCoreApplication::applicationDirPath()+filePath;
    // 创建DOM文档
    QDomDocument doc;


    // 创建根元素
    QDomElement root = doc.createElement("root");
    doc.appendChild(root);

    // 遍历外层QMap
    for (auto it = map.begin(); it != map.end(); ++it) {
        // 获取外层键
        QString outerKey = it.key();

        // 创建子元素
        QDomElement childElement = doc.createElement(outerKey);
        root.appendChild(childElement);

        // 遍历内层QMap
        for (auto innerIt = it.value().begin(); innerIt != it.value().end(); ++innerIt) {
            // 获取内层键和值
            QString innerKey = innerIt.key();
            QString innerValue = innerIt.value();

            // 创建次子元素
            QDomElement innerChildElement = doc.createElement(innerKey);
            QDomText innerChildText = doc.createTextNode(innerValue);
            innerChildElement.appendChild(innerChildText);
            childElement.appendChild(innerChildElement);
        }
    }

    // 写入文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return;
    }

    QTextStream out(&file);
    doc.save(out, 4); // 缩进为4个空格
    file.close();
}

QString Tool::numberVerify(QString string){
    const QStringList hexNumbers = string.split(' ');

    quint16 sum = 0;
    for (const QString &hexNumber : hexNumbers) {

        sum += hexNumber.toUInt(nullptr, 16);
    }

    return (string + " " + QString("%1").arg(sum, 2, 16, QChar('0')).right(2)).toUpper();
}

QString Tool::speedToQstring(double value){

    value = QString::number(value, 'f', 2).toFloat();

    union {
            quint32 value;
            quint8 bytes[4];
    } data;

    data.value = int(value);

    QString hexString;

    for (int i = 0; i < 4; ++i) {
        QString byteHex = QString("%1").arg(data.bytes[i], 2, 16, QChar('0')).toUpper();

        hexString += byteHex + (i < 3 ? " " : "");
    }

    return hexString;
}

QString Tool::lengthToQstring(double value){

    value = QString::number(value, 'f', 2).toFloat();

    union {
            quint64 value;
            quint8 bytes[4];
    } data;

    data.value = int(value);

    QString hexString;

    for (int i = 0; i < 8; ++i) {
        QString byteHex = QString("%1").arg(data.bytes[i], 2, 16, QChar('0')).toUpper();

        hexString += byteHex + (i < 7 ? " " : "");
    }

    return hexString;
}

QString Tool::crc16Ccitt(QString string)
{
    QByteArray builtData;
    bool ok = false;
    for (const auto &chunk : string.split(" "))
    {
        uchar byteValue = uchar(chunk.toUShort(&ok, 16));
        if (!ok)
        {
            // 转换失败后的操作，待定
            qDebug() << "weigh转换失败,数据存在错误" ;
            builtData.clear();
            continue;
        }
        else
        {
            builtData.append(char(byteValue));
        }
    }

    static const uint16_t crc16ModbusPoly = 0xa001;
    quint16 crc = 0xFFFF;

    for (int i = 0; i < builtData.size(); ++i)
    {
        char byte = builtData[i];

        for (int j = 0; j < 8; ++j)
        {
            bool bit = byte & 0x01;
            byte >>= 1;
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= crc16ModbusPoly;
            } else {
                crc >>= 1;
            }
            if (bit) {
                crc ^= crc16ModbusPoly;
            }
        }
    }


    QString temp =QString::number(crc,16);
    temp =temp.right(2)+" "+temp.left(temp.length()-2);

    return string + " " + temp;
}

bool Tool::addUser(User user,QMap<QString, QMap<QString, QString>> *usersMap){
    QStringList users= (*usersMap).keys();

    int max=0;
    foreach (QString item,users){
        if((*usersMap)[item]["name"]==user.name){
            return false;
        }
        int num = item.right(item.length()-4).toInt();

        max = num>max?num:max;
    }


    QString newUser = QStringLiteral("user%1").arg(max+1);
    user.userid = QString::number(max+1);

    (*usersMap)[newUser]["userid"]=user.userid;
    (*usersMap)[newUser]["name"]=user.name;
    (*usersMap)[newUser]["password"]=user.password;
    (*usersMap)[newUser]["role"]=user.role;
    (*usersMap)[newUser]["headerPic"]=user.headerPic;
    (*usersMap)[newUser]["lastTestTime"]=user.lastTestTime;
    (*usersMap)[newUser]["userDefaultName"]=user.userDefaultName;
    (*usersMap)[newUser]["userfilePath"]=user.userfilePath;

    writeXMlFile((*usersMap)["self"]["path"],*usersMap);

    return true;
}

User Tool::getUser(QString userName,QString password,QMap<QString, QMap<QString, QString>> usersMap){
    QStringList users= usersMap.keys();
    users.removeOne("self");
    User user;

    foreach (QString item,users){
        if(usersMap[item]["name"]==userName && usersMap[item]["password"]== password){
            user.userid = usersMap[item]["userid"];
            user.name = userName;
            user.password = password;
            user.role = usersMap[item]["role"];
            user.headerPic = usersMap[item]["headerPic"];
            user.lastTestTime = usersMap[item]["lastTestTime"];
            user.userDefaultName = usersMap[item]["userDefaultName"];
            user.userfilePath = usersMap[item]["userfilePath"];
            return user;
        }
    }
    return user;
}

User Tool::getUser(QString id,QMap<QString, QMap<QString, QString>>usersMap){
    QStringList users= usersMap.keys();
    users.removeOne("self");
    User user;

    foreach (QString item,users){

//        qDebug()<<usersMap[item]["userid"]<<id;

        if(usersMap[item]["userid"]==id){
            user.userid = usersMap[item]["userid"];
            user.name = usersMap[item]["name"];
            user.password = usersMap[item]["password"];
            user.role = usersMap[item]["role"];
            user.headerPic = usersMap[item]["headerPic"];
            user.lastTestTime = usersMap[item]["lastTestTime"];
            user.userDefaultName = usersMap[item]["userDefaultName"];
            user.userfilePath = usersMap[item]["userfilePath"];
            return user;
        }
    }
    return user;
}

bool Tool::freshUser(User user,QMap<QString, QMap<QString, QString>>* usersMap){
    User isUser = getUser(user.userid,*usersMap);

    if(isUser.userid!=user.userid)return false;
    else{
        QStringList users= (*usersMap).keys();
        users.removeOne("self");
        QString newUser;
        foreach (QString item,users){
            if((*usersMap)[item]["userid"]==user.userid){
                newUser = item;
                break;
            }
        }


        (*usersMap)[newUser]["userid"]=user.userid;
        (*usersMap)[newUser]["name"]=user.name;
        (*usersMap)[newUser]["password"]=user.password;
        (*usersMap)[newUser]["role"]=user.role;
        (*usersMap)[newUser]["headerPic"]=user.headerPic;
        (*usersMap)[newUser]["lastTestTime"]=user.lastTestTime;
        (*usersMap)[newUser]["userDefaultName"]=user.userDefaultName;
        (*usersMap)[newUser]["userfilePath"]=user.userfilePath;

        writeXMlFile((*usersMap)["self"]["path"],*usersMap);
        return true;
    }
}


bool Tool::deleteUser(User user,QMap<QString, QMap<QString, QString>>*usersMap){
    User isUser = getUser(user.name,user.password,*usersMap);
    if(isUser.name!=user.name)return false;
    return true;

}

bool Tool::saveCsvData(const QList<QList<float>> &data, const QList<QString> &header, QString filePath) {
    // 检查并创建目录
    QDir dir(QFileInfo(filePath).absolutePath());
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << dir.absolutePath();
            return false;
        }
    }

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        // 文件存在，生成新的文件名
        QString baseName = fileInfo.baseName();
        QString suffix = fileInfo.suffix();
        QString path = fileInfo.absolutePath();

        int count = 1;
        QString newFilePath = path + "/" + baseName + "_" + QString::number(count) + "." + suffix;
        while (QFileInfo(newFilePath).exists()) {
            ++count;
            newFilePath = path + "/" + baseName + "_" + QString::number(count) + "." + suffix;
        }
        filePath = newFilePath;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");  // 设置输出流的编码为 UTF-8

    // 输出表头
    for (const auto &h : header) {
        out << h << ",";
    }
    out << "\n";



    // 输出数据
    for (int i = 0; i < data[0].size(); ++i) {
        for (int j = 0; j < data.size(); ++j) {
            if (data[j].isEmpty()) continue;
            out << data[j][i] << ",";
        }
        out << "\n";
    }

    file.close();
    return true;
}

bool Tool::saveCsvData(const QList<QList<float>> &data, const QList<QString> &header, QMap<QString,QString>datails, QString filePath){
    // 检查并创建目录
    QDir dir(QFileInfo(filePath).absolutePath());
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << dir.absolutePath();
            return false;
        }
    }

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        // 文件存在，生成新的文件名
        QString baseName = fileInfo.baseName();
        QString suffix = fileInfo.suffix();
        QString path = fileInfo.absolutePath();

        int count = 1;
        QString newFilePath = path + "/" + baseName + "_" + QString::number(count) + "." + suffix;
        while (QFileInfo(newFilePath).exists()) {
            ++count;
            newFilePath = path + "/" + baseName + "_" + QString::number(count) + "." + suffix;
        }
        filePath = newFilePath;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");  // 设置输出流的编码为 UTF-8

    //文件介绍
    foreach(QString key,datails.keys()){
        out << key << "," << datails[key];
        out<<"\n";
    }
    out << "\n";

    // 输出表头
    for (const auto &h : header) {
        out << h << ",";
    }
    out << "\n";




    // 输出数据
    for (int i = 0; i < data[0].size(); ++i) {
        for (int j = 0; j < data.size(); ++j) {
            if (data[j].isEmpty()) continue;
            out << data[j][i] << ",";
        }
        out << "\n";
    }

    file.close();
    return true;
}

QList<QList<float>> Tool::loadCsvData(QString filePath, QList<QString> &header) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << file.errorString();
        return {};
    }

    QTextStream in(&file);

    QString line;
    while (header.length()<4) {
        line = in.readLine();
        header = line.split(',');
    }

    QList<QList<float>> data;

    for(int i = 0;i<header.length();i++){
        QList<float> row;
        data.append(row);
    }

    while (!in.atEnd()) {
        line = in.readLine();
        QStringList values = line.split(',');
        if(values.length()<3)continue;
        for(int i = 0;i<header.length();i++){
            data[i].append(values[i].toFloat());
        }
    }
    file.close();
    return data;
}


void Tool::dataSmooth(QList<float> &data, QString type, int window) {
    if (window <= 0 || window > data.size()) {
        qDebug() << "Invalid window size.";
        return;
    }

    QList<float> result;

    if (type == "平均加权") {
        for (int i = 0; i < data.size(); ++i) {
            float sum = 0.0f;
            int count = 0;
            for (int j = i - window / 2; j <= i + window / 2; ++j) {
                if (j >= 0 && j < data.size()) {
                    sum += data[j];
                    ++count;
                }
            }
            result.append(sum / count);
        }
    } else if (type == "中值平滑") {
        for (int i = 0; i < data.size(); ++i) {
            std::vector<float> windowValues;
            for (int j = i - window / 2; j <= i + window / 2; ++j) {
                if (j >= 0 && j < data.size()) {
                    windowValues.push_back(data[j]);
                }
            }
            std::sort(windowValues.begin(), windowValues.end());
            result.append(windowValues[windowValues.size() / 2]);
        }
    }

    data = result;
}


void Tool::interpolation(QList<float> &data,QString type,int count){
    if(type=="线性插值"){
        QList<float> interpolatedData;
        int n = data.size();

        if (n <= 1 || count <= 0) {
            qDebug() << "Invalid input data or count.";
            data = interpolatedData;
        }

        for(int i=0; i< n-1 ; i++){
            float step = (data[i+1]-data[i])/count;

            for(int j=0;j<count;j++){
                interpolatedData.append(data[i]+j*step);
            }
        }
        data = interpolatedData;
    }else if(type=="三次样条插值"){
        QList<float> interpolatedData;
        int n = data.size();

        if (n <= 1 || count <= 0) {
            qDebug() << "Invalid input data or count.";
            data = interpolatedData;
        }

        // 计算样条系数
        std::vector<std::vector<float>> A(n - 1, std::vector<float>(4));
        std::vector<float> b(n - 1);

        for (int i = 0; i < n - 1; ++i) {
            A[i][0] = 1;
            A[i][1] = static_cast<float>(i);
            A[i][2] = static_cast<float>(i * i);
            A[i][3] = static_cast<float>(i * i * i);

            b[i] = data[i + 1] - data[i];
        }

        // 使用高斯消元法求解系数
        std::vector<float> c = solveTridiagonalSystem(A, b);

        // 插值
        float step = static_cast<float>(n - 1) / (count - 1);

        for (int i = 0; i < count; ++i) {
            int index = static_cast<int>(i * step);
            float t = (i * step - index) / (index + 1 - index);

            float result = data[index] + c[index] * t +
                           c[index + 1] * t * t * (t - 1) / 2;

            interpolatedData.append(result);
        }

        data = interpolatedData;
    }

}

std::vector<float> Tool::solveTridiagonalSystem(const std::vector<std::vector<float>> &A, const std::vector<float> &b) {
    int n = A.size();
    std::vector<float> c(n - 1);

    // 创建一个新的矩阵来存储修改后的值
    std::vector<std::vector<float>> B(n, std::vector<float>(4));

    // 将原始矩阵复制到新矩阵中
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < 4; ++j) {
            B[i][j] = A[i][j];
        }
    }

    // 创建一个新的向量来存储修改后的 b 值
    std::vector<float> D(b.begin(), b.end());

    // 前进消元
    for (int i = 0; i < n - 1; ++i) {
        float a11 = B[i][0];
        float a12 = B[i][1];
        float a21 = B[i + 1][1];
        float a22 = B[i + 1][2];

        float factor = a21 / a11;
        B[i + 1][1] -= factor * a12;
        B[i + 1][2] -= factor * B[i][2];
        B[i + 1][3] -= factor * B[i][3];
        D[i + 1] -= factor * D[i];  // 使用新的向量 D

        c[i] = a12 / a11;
    }

    // 后向代入
    std::vector<float> x(n);
    x[n - 1] = D[n - 1] / B[n - 1][3];

    for (int i = n - 2; i >= 0; --i) {
        x[i] = (D[i] - B[i][3] * x[i + 1]) / B[i][2];
    }

    return x;
}

bool Tool::removeDirectoryRecursively(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        qDebug() << "目录不存在：" << path;
        return false;
    }

    // 遍历目录下的所有文件和子目录
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    foreach (const QFileInfo &entry, entries) {
        if (entry.isDir()) {
            // 递归删除子目录
            if (!removeDirectoryRecursively(entry.absoluteFilePath())) {
                return false;
            }
        } else {
            // 删除文件
            if (!QFile::remove(entry.absoluteFilePath())) {
                return false;
            }
        }
    }

    // 删除空目录
    return dir.rmdir(".");
}

void Tool::copyPathToClipboard(const QString &path) {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(path, QClipboard::Clipboard);
}
