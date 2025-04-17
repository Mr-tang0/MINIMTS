#include "loginwin.h"
#include "ui_loginwin.h"
#include <qtmaterialavatar.h>
#include <qtmaterialbadge.h>
#include "tool.h"
#include "mainwindow.h"

bool ensureDirectoryExists(const QString &filePath) {
    // 从文件路径中提取目录部分
    QFileInfo fileInfo(filePath);
    QDir directory = fileInfo.absoluteDir();

    // 如果目录不存在，则尝试创建
    if (!directory.exists()) {
        return directory.mkpath(".");
    }
    return true;
}


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

    // ui->login_userName->setText("tang");
    // ui->login_password->setText("123456");


}

loginWin::~loginWin()
{
    delete ui;
}

void loginWin::setHeader(QString path){
    if(path=="")path = ":/main/res/icon/sikh.jpg";
    m_avatar->setImage(QImage(path));
}

void loginWin::setCurrentUser(User user){
    user.lastTestTime = QDateTime::currentDateTime().toString();
    Tool::freshUser(user,&(MainWindow::usersMap));
    QString header = user.headerPic;
    if(!header.contains(":"))
        header = QCoreApplication::applicationDirPath()+header;
    if(header == "")
    {
        setHeader(":/main/res/icon/sikh.jpg");
        user.headerPic=":/main/res/icon/sikh.jpg";
    }
    else
        setHeader(header);

    ui->login_password->clear();
    ui->password->clear();
    ui->password_2->clear();
}

void loginWin::createImageWithInitial(const QString &userName, const QString &path) {
    if (userName.isEmpty()) return;

    QImage image(600, 600, QImage::Format_RGB32);
    image.fill(Qt::white);

    // 获取用户名的第一个字符并转换为大写
    QChar initial = userName.at(0).toUpper();

    // 创建画家对象用于在图像上绘制
    QPainter painter(&image);

    // 设置字体样式
    QFont font("Arial", 225, QFont::Bold); // 字体、字号和粗细可以根据需要调整
    painter.setFont(font);

    // 设置文字颜色为黑色
    painter.setPen(Qt::black);

    // 在图像中心绘制字符
    painter.drawText(image.rect(), Qt::AlignCenter, initial);

    // 结束绘制
    painter.end();

    ensureDirectoryExists(path);
    // 将图像保存到指定路径
    if (!image.save(path)) {
        qWarning("无法保存图像！");
    }
}

void loginWin::on_loginChecked_2_clicked()
{
    User user;

    user = Tool::getUser(ui->login_userName->text(),ui->login_password->text(),MainWindow::usersMap);

    if(user.name==ui->login_userName->text()){
        setCurrentUser(user);
        m_snackbar->addMessage("登录成功");
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

        createImageWithInitial(user.name,QCoreApplication::applicationDirPath()+"/root/cache/temp.jpg");
        user.headerPic= QCoreApplication::applicationDirPath()+"/root/cache/temp.jpg";

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
