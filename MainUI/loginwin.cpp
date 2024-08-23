#include "loginwin.h"
#include "ui_loginwin.h"
#include <qtmaterialavatar.h>
#include <qtmaterialbadge.h>
#include "tool.h"
#include "mainwindow.h"

loginWin::loginWin(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::loginWin),
    m_snackbar(new QtMaterialSnackbar),
    m_avatar(new QtMaterialAvatar()),
    m_badge(new QtMaterialBadge)
{
    ui->setupUi(this);

    m_snackbar->setParent(this);
    m_snackbar->setBoxWidth(200);
    m_snackbar->setAutoHideDuration(500);
//    m_snackbar->setFont(QFont("幼圆"));
    m_snackbar->setFontSize(10);

    m_avatar->setImage(QImage(":/main/res/icon/sikh.jpg"));
    m_avatar->setSize(100);

    QLayout *layout = ui->headPic->layout();
    if (layout == nullptr) {
        layout = new QVBoxLayout( ui->headPic);
    }
    layout->addWidget(m_avatar);
}

loginWin::~loginWin()
{
    delete ui;
}

void loginWin::on_loginChecked_2_clicked()
{
    User user;

    user = Tool::getUser(ui->login_userName->text(),ui->login_password->text(),MainWindow::usersMap);

    if(user.name==ui->login_userName->text()){

        user.lastTestTime = QDateTime::currentDateTime().toString();
        Tool::freshUser(user,&(MainWindow::usersMap));
        m_snackbar->addMessage("登录成功");
        QString header = QCoreApplication::applicationDirPath()+user.headerPic;
        m_avatar->setImage(QImage(header));
        emit logined(user);
    }else
        m_snackbar->addMessage("登录失败");
}
