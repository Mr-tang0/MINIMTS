#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCore>
#include <QtGui>

// 辅助类与组件
// #include "iconhelper.h"
// #include "quihelper.h"
// #include "qtmaterialsnackbar.h"
// #include "tool.h"

// 业务窗口
// #include "loginwin.h"
#include "onlinelogin.h"
#include "mainwidget.h"
#include "systemwidget.h"
#include "aboutwidget.h"
#include "userwidget.h"

#include "minimtsuser.h"

class QtMaterialAvatar;
class QtMaterialBadge;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    static MINIMTSUser *user;
    void runUpdateCommand();

private slots:
    // 菜单按钮事件
    void on_btnMenu_Min_clicked();
    void on_btnMenu_Max_clicked();
    void on_btnMenu_Close_clicked();
    void buttonClick();

private:
    // 初始化方法
    void initThis();
    void initConnections();
    void initStyle();

    // 错误信息管理
    void addErrMessage(QString msg);
    void deleteErrMessage(QString msg);

    // 成员变量
    Ui::MainWindow      *ui;
    QtMaterialSnackbar  *const m_snackbar;
    QtMaterialAvatar    *const m_avatar;
    QtMaterialBadge     *const m_badge;

    int oldAxis = 0;
    QPoint lastPos;
    bool filteropen = true;

    // 子界面组件
    systemWidget *system;
    OnlineLogin  *onlinelogin;
    mainWidget   *mainwd;
    aboutWidget  *about;
    userWidget   *userWin;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override {
        lastPos = event->globalPos();
        QMainWindow::mousePressEvent(event);
    }
};

#endif // MAINWINDOW_H
