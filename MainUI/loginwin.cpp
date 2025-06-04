#include "loginwin.h"
#include "ui_loginwin.h"
#include <qtmaterialavatar.h>
#include <qtmaterialbadge.h>
#include "tool.h"
#include "xmlreader.h"
#include <QRegularExpressionValidator>

QMap<QString, QMap<QString, QString>> loginWin::usersMap;

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
    ui->userName->setValidator(validator);
    ui->login_userName->setValidator(validator);


    connect(ui->commandLinkButton,&QCommandLinkButton::clicked,[=](){
        if(ui->commandLinkButton->text()=="没有账号？"){
            ui->stackedWidget->setCurrentIndex(0);
            ui->commandLinkButton->setText("返回登录");

            //tab顺序
            ui->userName->setFocusPolicy(Qt::StrongFocus);
            ui->password->setFocusPolicy(Qt::StrongFocus);
            ui->password_2->setFocusPolicy(Qt::StrongFocus);

            setTabOrder(ui->userName, ui->password);
            setTabOrder(ui->password, ui->password_2);
            setTabOrder(ui->password_2, ui->userName);

            ui->login_password->setFocusPolicy(Qt::NoFocus);
            ui->login_userName->setFocusPolicy(Qt::NoFocus);

            ui->userName->setFocus();
        }else{
            ui->stackedWidget->setCurrentIndex(1);
            ui->commandLinkButton->setText("没有账号？");
            //tab顺序
            ui->login_userName->setFocusPolicy(Qt::StrongFocus);
            ui->login_password->setFocusPolicy(Qt::StrongFocus);

            setTabOrder(ui->login_password, ui->login_userName);
            setTabOrder(ui->login_userName, ui->login_password);

            ui->password->setFocusPolicy(Qt::NoFocus);
            ui->password_2->setFocusPolicy(Qt::NoFocus);
            ui->userName->setFocusPolicy(Qt::NoFocus);

            ui->login_userName->setFocus();
        }
    });

    //tab顺序
    setTabOrder(ui->login_password, ui->login_userName);
    setTabOrder(ui->login_userName, ui->login_password);

    ui->password->setFocusPolicy(Qt::NoFocus);
    ui->password_2->setFocusPolicy(Qt::NoFocus);
    ui->userName->setFocusPolicy(Qt::NoFocus);

    ui->loginChecked->setFocusPolicy(Qt::NoFocus);
    ui->loginChecked_2->setFocusPolicy(Qt::NoFocus);
    ui->commandLinkButton->setFocusPolicy(Qt::NoFocus);

    ui->login_userName->setFocus();

    // ui->login_userName->setText("tang");
    // ui->login_password->setText("111111");


    //从文件提取所有用户信息
    rootPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +"/Tang/MiniMTS/Users";
    usersMap = XMLReader::Read_XML_File(rootPath+"/users.xml");
    usersMap["self"]["path"]=rootPath+"/users.xml";

    //从云端获取用户信息



}

loginWin::~loginWin()
{
    delete ui;
}

void loginWin::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        onEnterPressed();
    }
    QWidget::keyPressEvent(event);

}

void loginWin::setHeader(QString path){
    if(path=="")path = ":/main/res/icon/user.png";
    m_avatar->setImage(QImage(path));
}

void loginWin::setCurrentUser(User user){
    user.lastTestTime = QDateTime::currentDateTime().toString();
    Tool::freshUser(user,&usersMap);
    QString header = user.headerPic;
    if(!header.contains(":"))
        header = rootPath + header;
    if(header == "")
    {
        setHeader(":/main/res/icon/user.png");
        user.headerPic=":/main/res/icon/user.png";
    }
    else
        setHeader(header);

    ui->login_password->clear();
    ui->password->clear();
    ui->password_2->clear();
}

//绘制头像
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

void loginWin::onEnterPressed() {
    if(ui->stackedWidget->currentWidget()->objectName()=="loginWin1")
        on_loginChecked_2_clicked();
    else if(ui->stackedWidget->currentWidget()->objectName()=="registerWin")
        on_loginChecked_clicked();
}

void loginWin::on_loginChecked_2_clicked()
{
    User user;

    user = Tool::getUser(ui->login_userName->text(),ui->login_password->text(),usersMap);

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

    user = Tool::getUser(ui->userName->text(),ui->password->text(),usersMap);

    if(user.name==ui->userName->text()){
        m_snackbar->addMessage("用户已存在");
    }else
    {
        user.name=ui->userName->text();
        user.password=ui->password->text();
        user.role="user";

        createImageWithInitial(user.name,rootPath+"/"+user.name+"/header.jpg");
        user.headerPic= rootPath+"/"+user.name+"/header.jpg";

        user.userfilePath = rootPath+"/"+user.name;
        user.userDefaultName = "userData-"+user.name;

        bool flag = Tool::addUser(user,&usersMap);

        user = Tool::getUser(user.name,user.password,usersMap);

        if(flag){
            m_snackbar->addMessage("注册成功");
            QString header = user.headerPic;
            if(!header.contains(":"))header = rootPath+header;
            m_avatar->setImage(QImage(header));
            Tool::delay(500);
            emit logined(user);
        }
    }

}
