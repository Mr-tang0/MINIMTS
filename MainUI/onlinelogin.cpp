#include "onlinelogin.h"
#include "ui_onlinelogin.h"
#include <qtmaterialavatar.h>
#include <qtmaterialbadge.h>
#include <QRegularExpressionValidator>
#include "mainwindow.h"
#include <QMessageBox>

OnlineLogin::OnlineLogin(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OnlineLogin),
    m_snackbar(new QtMaterialSnackbar),
    m_avatar(new QtMaterialAvatar()),
    m_badge(new QtMaterialBadge)
{
    ui->setupUi(this);

    this->setWindowTitle("登录");

    m_snackbar->setParent(this);
    m_snackbar->setBoxWidth(200);
    m_snackbar->setAutoHideDuration(500);
    m_snackbar->setFontSize(10);

    m_avatar->setImage(QImage(":/main/res/icon/user.png"));
    m_avatar->setSize(150);

    QLayout *layout = ui->headPic->layout();
    if (layout == nullptr) {
        layout = new QVBoxLayout( ui->headPic);
    }
    layout->addWidget(m_avatar);

    QRegularExpression re("[a-zA-Z]*");
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(re, this);
    ui->loginName->setValidator(validator);
    ui->registerName->setValidator(validator);


    connect(ui->commandLinkButton,&QCommandLinkButton::clicked,[=](){
        if(ui->commandLinkButton->text()=="没有账号？"){
            ui->stackedWidget->setCurrentIndex(0);
            ui->commandLinkButton->setText("返回登录");

            //tab顺序
            ui->registerName->setFocusPolicy(Qt::StrongFocus);
            ui->registerPassword->setFocusPolicy(Qt::StrongFocus);
            ui->registerPasswordCheck->setFocusPolicy(Qt::StrongFocus);

            setTabOrder(ui->registerName, ui->registerPassword);
            setTabOrder(ui->registerPassword, ui->registerPasswordCheck);
            setTabOrder(ui->registerPasswordCheck, ui->registerName);

            ui->loginPassword->setFocusPolicy(Qt::NoFocus);
            ui->loginName->setFocusPolicy(Qt::NoFocus);

            ui->registerName->setFocus();
        }else{
            ui->stackedWidget->setCurrentIndex(1);
            ui->commandLinkButton->setText("没有账号？");
            //tab顺序
            ui->loginName->setFocusPolicy(Qt::StrongFocus);
            ui->loginPassword->setFocusPolicy(Qt::StrongFocus);

            setTabOrder(ui->loginPassword, ui->loginName);
            setTabOrder(ui->loginName, ui->loginPassword);

            ui->registerName->setFocusPolicy(Qt::NoFocus);
            ui->registerPassword->setFocusPolicy(Qt::NoFocus);
            ui->registerPasswordCheck->setFocusPolicy(Qt::NoFocus);

            ui->loginName->setFocus();
        }
    });

    //tab顺序
    setTabOrder(ui->registerPassword, ui->loginName);
    setTabOrder(ui->loginName, ui->registerPassword);

    ui->registerName->setFocusPolicy(Qt::NoFocus);
    ui->registerPassword->setFocusPolicy(Qt::NoFocus);
    ui->registerPasswordCheck->setFocusPolicy(Qt::NoFocus);

    ui->commandLinkButton->setFocusPolicy(Qt::NoFocus);

    ui->loginName->setFocus();

    connect(MainWindow::user,&PimsUser::faild,this,[=](QString msg){
        m_snackbar->addMessage(msg);
    });


}

OnlineLogin::~OnlineLogin()
{
    qDebug()<<"tuichu";
    delete ui;
}

void OnlineLogin::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        onEnterPressed();
    }
    QWidget::keyPressEvent(event);

}

void OnlineLogin::setHeader(QString path){
    if(path=="")path = ":/main/res/icon/user.png";
    m_avatar->setImage(QImage(path));
}

void OnlineLogin::setCurrentUser(MINIMTSUser *user){
    setHeader(user->header);

    ui->loginPassword->clear();
    ui->registerPassword->clear();
    ui->registerPasswordCheck->clear();

    ui->stackedWidget->setCurrentWidget(ui->loginWin);
}



void OnlineLogin::onEnterPressed() {
    if(ui->stackedWidget->currentWidget()->objectName()=="loginWin")
        on_loginBtn_clicked();
    else if(ui->stackedWidget->currentWidget()->objectName()=="registerWin")
        on_registerBtn_clicked();
}



void OnlineLogin::on_loginBtn_clicked()
{
    QString name = ui->loginName->text().trimmed();
    QString pwd = ui->loginPassword->text();

    if (name.isEmpty() || pwd.isEmpty()) {
        m_snackbar->addMessage("用户名或密码不能为空！");
        return;
    }

    MainWindow::user->Name = name;
    MainWindow::user->Password = pwd;
    MainWindow::user->UserLogin();
}


void OnlineLogin::on_registerBtn_clicked()
{
    QString name = ui->registerName->text().trimmed();
    QString pwd = ui->registerPassword->text();
    QString pwdCheck = ui->registerPasswordCheck->text();
    QString email = ui->registerEmail->text().trimmed();

    if (name.isEmpty() || pwd.isEmpty() || email.isEmpty()) {
        m_snackbar->addMessage("请完整填写所有注册信息！");
        return;
    }

    if (pwd != pwdCheck) {
        m_snackbar->addMessage("两次输入的密码不一致！");
        return;
    }
    if (pwd.length() < 6) {
        m_snackbar->addMessage("密码长度至少需要6位！");
        return;
    }

    if (!email.contains('@') || !email.contains('.')) {
        m_snackbar->addMessage("请输入有效的邮箱地址！");
        return;
    }

    MainWindow::user->Name = name;
    MainWindow::user->Password = pwd;
    MainWindow::user->Email = email;

    MainWindow::user->UserRegister();
}

