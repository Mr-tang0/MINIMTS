#include "minimtsuser.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QImage>
#include <QPainter>


MINIMTSUser::MINIMTSUser(QObject *parent)
    : PimsUser{parent}
{
}

//根据user_json将在线信息下载到本地
void MINIMTSUser::getOnlineDetailToLocal(){
    //下载static_path里的minimts_project.xml到本地下：QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +"/Tang/MiniMTS/Current";
    if (this->id().isEmpty()) return;

    // 确定本地保存路径
    QString localDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Tang/MiniMTS/Current";
    QDir dir;
    if (!dir.exists(localDir)) {
        dir.mkpath(localDir); // 确保本地目录存在
    }
    QString localFilePath = localDir + "/minimts_project.xml";

    // 构建下载 URL
    // 接口格式：/download/用户ID/文件名
    QString urlStr = QString(server+"/download/%1/minimts_project.xml").arg(this->id());
    QUrl url(urlStr);

    // 发送请求
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkRequest request(url);

    qDebug() << "开始下载配置文件:" << urlStr;

    QNetworkReply *reply = manager->get(request);

    // 处理下载结果
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            // 获取数据
            QByteArray data = reply->readAll();

            // 写入文件
            QFile file(localFilePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                qDebug() << "配置文件下载并保存成功:" << localFilePath;
            } else {
                qDebug() << "本地文件打开失败，无法保存";
            }
        } else if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
            qDebug() << "服务器暂无此用户的配置文件，已略过。";
        } else {
            qDebug() << "下载出错:" << reply->errorString();
        }

        reply->deleteLater();
        manager->deleteLater();
    });
}

//上传到云端
void MINIMTSUser::updateLocalDetailToCloud(){
    if (this->id().isEmpty()) return;

    // 1. 确定本地文件路径
    QString localFilePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
                            "/Tang/MiniMTS/Current/minimts_project.xml";

    QFile *file = new QFile(localFilePath);
    if (!file->exists() || !file->open(QIODevice::ReadOnly)) {
        qDebug() << "本地无配置文件或无法读取，取消上传:" << localFilePath;
        delete file;
        return;
    }

    // 2. 构建 MultiPart 请求
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    // name 必须与 Flask 中 request.files['file'] 的键名一致
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\"; filename=\"minimts_project.xml\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart); // 这样 multiPart 被删除时文件也会被关闭和删除

    multiPart->append(filePart);

    // 3. 构建请求 URL
    QString urlStr = QString(server + "/upload/%1").arg(this->id());
    QUrl url(urlStr);
    QNetworkRequest request(url);

    // 4. 发送请求
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->post(request, multiPart);

    qDebug() << "开始同步配置到云端..." << urlStr;

    // 5. 处理上传结果
    connect(reply, &QNetworkReply::finished, this, [=]() {
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "云端备份成功！";
        } else {
            qDebug() << "上传失败:" << reply->errorString();
            qDebug() << "服务器响应:" << reply->readAll();
        }

        // 释放资源
        reply->deleteLater();
        multiPart->deleteLater();
        manager->deleteLater();
    });
}

void MINIMTSUser::creatLocal(){
    // 确定本地路径
    QString localFilePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Tang/MiniMTS/Users/"+Name;
    QDir dir;
    if (!dir.exists(localFilePath)) {
        dir.mkpath(localFilePath); // 确保本地目录存在
        createImageWithInitial(Name, localFilePath+"/header.png");
    }

    localPath = localFilePath;
    header = localFilePath+"/header.png";
}


void MINIMTSUser::createImageWithInitial(const QString &userName, const QString &path) {
    if (userName.isEmpty()) return;

    QImage image(600, 600, QImage::Format_RGB32);
    image.fill(Qt::white);

    // 获取用户名的第一个字符并转换为大写
    QChar initial = userName.at(0).toUpper();

    // 创建画家对象用于在图像上绘制
    QPainter painter(&image);

    // 设置字体样式
    QFont font("Arial", 225, QFont::Bold); // 字体、字号和粗细可以根据需要调整
    painter.setFont(font);

    // 设置文字颜色为黑色
    painter.setPen(Qt::black);

    // 在图像中心绘制字符
    painter.drawText(image.rect(), Qt::AlignCenter, initial);

    // 结束绘制
    painter.end();

    QFileInfo fileInfo(path);
    QDir directory = fileInfo.absoluteDir();

    // 如果目录不存在，则尝试创建
    if (!directory.exists()) {
        directory.mkpath(path);
    }

    // 将图像保存到指定路径
    if (!image.save(path)) {
        qWarning("无法保存图像！");
    }
}
