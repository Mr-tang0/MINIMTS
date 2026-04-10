#ifndef USERWIDGET_H
#define USERWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include "minimtsuser.h"

class QtMaterialAvatar;
class QtMaterialBadge;

namespace Ui {
class userWidget;
}



class userWidget : public QWidget
{
    Q_OBJECT

public:
    explicit userWidget(QWidget *parent = nullptr);
    ~userWidget();

    void freshUserDetails(MINIMTSUser *user);
    void chooseHeader();
    void setupContextMenu();

private slots:
    void on_exitUser_clicked();

private:
    Ui::userWidget *ui;
    QtMaterialAvatar      *const m_avatar;
    QtMaterialBadge       *const m_badge;

    QString tempHeaderPath = "";
    QString rootPath = "";

protected:
    bool eventFilter(QObject *obj, QEvent *event);


signals:
    void message(QString);
    void WanaExit();
};

#endif // USERWIDGET_H
