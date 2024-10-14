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
    m_avatar->setSize(150);

    QLayout *layout = ui->headPic->layout();
    if (layout == nullptr) {
        layout = new QVBoxLayout( ui->headPic);
    }
    layout->addWidget(m_avatar);

    connect(ui->commandLinkButton,&QCommandLinkButton::clicked,[=](){
        if(ui->commandLinkButton->text()=="没有账号？"){
            ui->stackedWidget->setCurrentIndex(0);
            ui->commandLinkButton->setText("返回登录");
        }else{
            ui->stackedWidget->setCurrentIndex(1);
            ui->commandLinkButton->setText("没有账号？");
        }
    });
    ui->login_userName->setText("tang");
    ui->login_password->setText("123456");


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
        QString header = user.headerPic;
        if(!header.contains(":"))header = QCoreApplication::applicationDirPath()+header;
        m_avatar->setImage(QImage(header));
        Tool::delay(500);
        emit logined(user);
    }else
        m_snackbar->addMessage("登录失败");
}

void loginWin::on_loginChecked_clicked()
{
    if(ui->password->text().length()<6){
        m_snackbar->addMessage("密码不得少于6位");
        return;
    }

    if(ui->password->text()!=ui->password_2->text()){
        m_snackbar->addMessage("两次输入密码不一致");
        return;
    }
    User user;
    user = Tool::getUser(ui->userName->text(),ui->password->text(),MainWindow::usersMap);
    if(user.name==ui->userName->text()){
        m_snackbar->addMessage("用户已存在");
    }else
    {
        user.name=ui->userName->text();
        user.password=ui->password->text();
        user.role="user";
        user.headerPic=":/main/res/icon/sikh.jpg";
        user.userfilePath = QCoreApplication::applicationDirPath()+"/root/users/"+user.name;
        user.userDefaultName = "userData-"+user.name;

        bool flag = Tool::addUser(user,&MainWindow::usersMap);
        user = Tool::getUser(user.name,user.password,MainWindow::usersMap);
        if(flag){
            m_snackbar->addMessage("注册成功");
            QString header = user.headerPic;
            if(!header.contains(":"))header = QCoreApplication::applicationDirPath()+header;
            m_avatar->setImage(QImage(header));
            Tool::delay(500);
            emit logined(user);
        }
    }

}
