#ifndef MINIMTSUSER_H
#define MINIMTSUSER_H

#include <QObject>
#include <pimsuser.h>

class MINIMTSUser : public PimsUser
{
    Q_OBJECT
public:
    explicit MINIMTSUser(QObject *parent = nullptr);

    QString localPath = "";
    QString header = "";

public slots:
    //根据user_json将在线信息下载到本地
    void getOnlineDetailToLocal();

    //上传到云端
    void updateLocalDetailToCloud();

    //根据user_json将在线信息下载到本地
    void creatLocal();

    //绘制头像
    void createImageWithInitial(const QString &userName, const QString &path);
};

#endif // MINIMTSUSER_H
