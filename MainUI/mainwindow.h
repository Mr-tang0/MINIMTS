#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCore>
#include <QtGui>

#include "iconhelper.h"
#include "quihelper.h"
#include "qtmaterialsnackbar.h"

#include "loginwin.h"
#include "mainwidget.h"
#include "systemwidget.h"
#include "aboutwidget.h"
#include "userwidget.h"
#include "tool.h"

class QtMaterialAvatar;
class QtMaterialBadge;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    static QMap<QString, QMap<QString, QString>> rootMap;
    static QMap<QString, QMap<QString, QString>> systemMap;
    static QMap<QString, QMap<QString, QString>> cmdMap;
    static QMap<QString, QMap<QString, QString>> usersMap;
    static User user;

private:
    Ui::MainWindow *ui;
    QtMaterialSnackbar  *const m_snackbar;
    QtMaterialAvatar      *const m_avatar;
    QtMaterialBadge       *const m_badge;

    void initThis();
    loginWin *login = new loginWin;
    mainWidget *main = new mainWidget;
    systemWidget *system = new systemWidget;
    aboutWidget *about = new aboutWidget;
    userWidget *userWin = new userWidget;


protected:
    bool filteropen = true;
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void on_btnMenu_Min_clicked();
    void on_btnMenu_Max_clicked();
    void on_btnMenu_Close_clicked();
    void buttonClick();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        // 记录鼠标按下位置
        lastPos = event->globalPos();
        QMainWindow::mousePressEvent(event);
    }


private:
    QPoint lastPos;

};
#endif // MAINWINDOW_H
