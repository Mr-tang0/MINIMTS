#ifndef USERWIDGET_H
#define USERWIDGET_H

#include <QWidget>
#include "tool.h"
#include <QTreeView>
#include <QFileSystemModel>


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

    void freshUserDetails(User user);
    void chooseHeader();
    void setupContextMenu();

private slots:
    void on_pushButton_clicked();

    void on_headerChoose_clicked();

    void on_pushButton_2_clicked();

private:
    Ui::userWidget *ui;
    QtMaterialAvatar      *const m_avatar;
    QtMaterialBadge       *const m_badge;

    QString tempHeaderPath = "";

signals:
    void message(QString);
};

#endif // USERWIDGET_H
