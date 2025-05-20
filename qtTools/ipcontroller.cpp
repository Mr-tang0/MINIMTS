#include "ipcontroller.h"
#include <QNetworkInterface>
#include <QString>
#include <QProcess>


IPController::IPController(QObject *parent)
    : QObject{parent}
{}

QMap<QString, QString> IPController::get_system_current_IP(){
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    QMap<QString, QString> ipAddresses;

    for (const QNetworkInterface &interface : interfaces) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack) &&
            !interface.hardwareAddress().isEmpty()) {

            QList<QNetworkAddressEntry> entries = interface.addressEntries();

            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    ipAddresses.insert(interface.humanReadableName(), entry.ip().toString());
                    break;
                }
            }
        }
    }
    return ipAddresses; // 返回包含网卡名和对应IP地址的映射
}

bool IPController::change_ip(QString interface_name, QString ip){
    QProcess process;
    // 使用 'ip' 工具设置新的IP地址，假设子网掩码为255.255.255.0 (或者 /24)
    QString program = "ip";
    QStringList arguments;
    arguments << "addr" << "change" << ip + "/24" << "dev" << interface_name;

    process.start(program, arguments);
    if(!process.waitForStarted()) {
        return false;
    }
    if(!process.waitForFinished()) {
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        // 如果退出状态不是正常退出或退出代码不是0，则表示发生了错误
        return false;
    }

    return true;
}
