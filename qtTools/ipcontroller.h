#ifndef IPCONTROLLER_H
#define IPCONTROLLER_H

#include <QObject>

class IPController : public QObject
{
    Q_OBJECT
public:
    explicit IPController(QObject *parent = nullptr);

    void check_IP(QString target_ip);

    QMap<QString, QString> get_system_current_IP();

    bool change_ip(QString interface_name, QString ip);

signals:
};

#endif // IPCONTROLLER_H
