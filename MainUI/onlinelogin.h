#ifndef ONLINELOGIN_H
#define ONLINELOGIN_H


#include <QWidget>
#include "qtmaterialsnackbar.h"
#include <QPainter>
#include <QKeyEvent>
#include "minimtsuser.h"

class QtMaterialAvatar;
class QtMaterialBadge;



namespace Ui {
class OnlineLogin;
}

class OnlineLogin : public QWidget
{
    Q_OBJECT

public:
    explicit OnlineLogin(QWidget *parent = nullptr);
    ~OnlineLogin();

    void setCurrentUser(MINIMTSUser *user);

protected:
    void keyPressEvent(QKeyEvent *event) override ;

private slots:
    void onEnterPressed();
    void on_loginBtn_clicked();
    void on_registerBtn_clicked();
    void setHeader(QString path);

private:
    Ui::OnlineLogin *ui;
    QtMaterialSnackbar    *const m_snackbar;
    QtMaterialAvatar      *const m_avatar;
    QtMaterialBadge       *const m_badge;

    QString rootPath ;


};

#endif // ONLINELOGIN_H
