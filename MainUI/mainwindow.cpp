#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <qtmaterialavatar.h>
#include <qtmaterialbadge.h>

User MainWindow::user;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
      m_snackbar(new QtMaterialSnackbar),
      m_avatar(new QtMaterialAvatar(QImage(":/main/res/icon/sikh.jpg"))),
      m_badge(new QtMaterialBadge)
{
    ui->setupUi(this);

    m_snackbar->setParent(this);
    m_snackbar->setAutoHideDuration(400);
    m_snackbar->setFont(QFont("幼圆"));
    m_snackbar->setFontSize(12);


    m_avatar->setSize(60);
    m_badge->setParent(m_avatar);
    m_badge->setRelativePosition(18, 12);
    m_badge->setText("0");
    m_badge->setEnabled(false);


    QLayout *layout = ui->header->layout();
    if (layout == nullptr) {
        layout = new QVBoxLayout( ui->header);
    }
    layout->addWidget(m_avatar);

    initThis();

    connect(mainwd->minilogic,&MiniMTSLogic::LimitOccured,this, [=](int axis){
        if (axis!=oldAxis){
            oldAxis = axis;

            if (axis!=0)
            {
                addErrMessage(QStringLiteral("限位传感器被触发:方向 %1").arg(axis));
                mainwd->limitStopOption(true,axis);
            }
            else
            {
                deleteErrMessage(QStringLiteral("限位传感器被触发:方向 %1").arg(axis));
                mainwd->limitStopOption(false, axis);
            }
        }
    });

    connect(mainwd,&mainWidget::errOccured,this, [=](QString err){
        addErrMessage(err);
    });

    connect(userWin,&userWidget::WanaExit,this,[=](){
        this->close();
        login->show();
        login->setCurrentUser(user);
        m_snackbar->addMessage("退出成功");
    });

    login->setHeader("");

    //自动更新判断
    if(system->systemMap["advance"]["autoFindVersion"]=="是")
        runUpdateCommand();

    login->show();

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::runUpdateCommand(){
    QString appDirPath = QCoreApplication::applicationDirPath();

    qDebug()<<"findNew";

    // 构造命令和参数
    QString program = appDirPath + "/update";
    QStringList arguments;
    arguments << "https://raw.gitcode.com/m0_51547074/update_info/raw/main/minimtsinfo.json"
              << "MINIMTS"
              << "2";

    QProcess process;
    process.setWorkingDirectory(appDirPath);
    process.start(program, arguments);

    // 等待进程启动成功
    if (!process.waitForStarted()) {
        qDebug() << "无法启动 update 程序";
        return;
    }

    //超时时间300s
    if (!process.waitForFinished(3000000)) {
        qDebug() << "执行过程中发生错误：" << process.errorString();
        return;
    }
}

void MainWindow::addErrMessage(QString msg)
{
    int errCount = m_badge->text().toInt();
    errCount += 1;
    m_badge->setText(QString::number(errCount));
    m_badge->setEnabled(true);
}

void MainWindow::deleteErrMessage(QString){
    int errCount = m_badge->text().toInt();
    if (errCount>1)
    {
        errCount -= 1;
        m_badge->setText(QString::number(errCount));
        m_badge->setEnabled(true);
    }else{
        m_badge->setText(QString::number(0));
        m_badge->setEnabled(false);
    }

}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(!filteropen)return false;

    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == ui->widgetTitle) {
            on_btnMenu_Max_clicked();
            return true;
        }
    }
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        const int topMargin = 5; // 距离顶部的边缘距离
        bool isNearTopEdge = (mouseEvent->globalY() <= topMargin);


        if (isNearTopEdge && windowState() != Qt::WindowMaximized) {
            // 鼠标靠近顶部时最大化
            ui->btnMenu_Max->click();
            showMaximized();
            filteropen=false;
            Tool::delay(500);
            filteropen=true;
        } else if (!isNearTopEdge && windowState() == Qt::WindowMaximized) {
            // 鼠标离开顶部时还原
            ui->btnMenu_Max->click();
            filteropen=false;
            Tool::delay(500);
            filteropen=true;
        }
        lastPos = mouseEvent->globalPos();
    }
    return QWidget::eventFilter(watched, event);
}

void MainWindow::initThis(){

    QUIHelper::setFramelessForm(this);

    this->centralWidget()->layout()->setMargin(1);

    IconHelper::setIcon(ui->btnMenu_Min, 0xf2d1);
    IconHelper::setIcon(ui->btnMenu_Max, 0xf2d2);
    IconHelper::setIcon(ui->btnMenu_Close, 0xf00d);

    ui->widgetTitle->installEventFilter(this);
    ui->widgetTitle->setProperty("form", "title");
    ui->widgetTop->setProperty("nav", "top");

    QFont font;
    font.setPixelSize(25);
    ui->labTitle->setFont(font);
    ui->labTitle->setText("MINIMTS");
    this->setWindowTitle(ui->labTitle->text());

    ui->stackedWidget->setStyleSheet("font:13px;");



    QList<QWidget*> widgetList;

    system = new systemWidget;
    login = new loginWin;
    mainwd = new mainWidget;
    about = new aboutWidget;
    userWin = new userWidget;

    widgetList<<mainwd<<system<<about<<userWin;


    mainwd->initSystem();

    connect(mainwd,&mainWidget::connected,[=](bool flag){
        if(flag){
            m_snackbar->addMessage("连接成功！");
        }else m_snackbar->addMessage("连接失败！");

    });

    connect(mainwd,&mainWidget::disconnected,[=](){m_snackbar->addMessage("已断开！");});

    connect(mainwd,&mainWidget::message,[=](QString message){ m_snackbar->addMessage(message);});


    connect(system,&systemWidget::saved,[=](bool flag){
        if(flag){
            m_snackbar->addMessage("保存成功！");
            mainwd->initSystem();
        }
        ui->btnMain->click();
    });

    for (int i = 0; i <widgetList.length(); i++) {
        QWidget *page = ui->stackedWidget->widget(i);

        QLayout *layout = page->layout();
        if (layout == nullptr) {
            layout = new QVBoxLayout(page); // 使用QVBoxLayout作为示例布局
        }
        layout->addWidget(widgetList[i]);
        page->update();
    }

    int addWidth = 20;
    int addHeight = 10;
    int rbtnWidth = 15;
    int ckWidth = 13;
    int scrWidth = 12;
    int borderWidth = 3;

    QStringList qss;
    qss << QString("QComboBox::drop-down,QDateEdit::drop-down,QTimeEdit::drop-down,QDateTimeEdit::drop-down{width:%1px;}").arg(addWidth);
    qss << QString("QComboBox::down-arrow,QDateEdit[calendarPopup=\"true\"]::down-arrow,QTimeEdit[calendarPopup=\"true\"]::down-arrow,"
                   "QDateTimeEdit[calendarPopup=\"true\"]::down-arrow{width:%1px;height:%1px;right:2px;}").arg(addHeight);
    qss << QString("QRadioButton::indicator{width:%1px;height:%1px;}").arg(rbtnWidth);
    qss << QString("QCheckBox::indicator,QGroupBox::indicator,QTreeWidget::indicator,QListWidget::indicator{width:%1px;height:%1px;}").arg(ckWidth);
    qss << QString("QScrollBar:horizontal{min-height:%1px;border-radius:%2px;}QScrollBar::handle:horizontal{border-radius:%2px;}"
                   "QScrollBar:vertical{min-width:%1px;border-radius:%2px;}QScrollBar::handle:vertical{border-radius:%2px;}").arg(scrWidth).arg(scrWidth / 2);
    qss << QString("QWidget#widget_top>QToolButton:pressed,QWidget#widget_top>QToolButton:hover,"
                   "QWidget#widget_top>QToolButton:checked,QWidget#widget_top>QLabel:hover{"
                   "border-width:0px 0px %1px 0px;}").arg(borderWidth);
    qss << QString("QWidget#widgetleft>QPushButton:checked,QWidget#widgetleft>QToolButton:checked,"
                   "QWidget#widgetleft>QPushButton:pressed,QWidget#widgetleft>QToolButton:pressed{"
                   "border-width:0px 0px 0px %1px;}").arg(borderWidth);
    this->setStyleSheet(qss.join(""));

    qDebug()<<"dd";

    QSize icoSize(32, 32);
    int icoWidth = 85;

    QList<QToolButton *> tbtns = ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *btn, tbtns) {
        btn->setIconSize(icoSize);

        btn->setMinimumWidth(icoWidth);
        btn->setCheckable(true);
        connect(btn, SIGNAL(clicked()), this, SLOT(buttonClick()));
    }
    ui->btnMain->click();


    connect(mainwd,&mainWidget::btnClicked,[=](QString btn){
       if(btn=="toSet")ui->btnConfig->click();
    });

    connect(login,&loginWin::logined,[=](User newUser){
        this->user = newUser;

        QString header = this->user.headerPic;
        if(!header.contains(":"))header = QCoreApplication::applicationDirPath()+header;

        m_avatar->setImage(QImage(header));

        this->show();
        login->hide();
        userWin->freshUserDetails(this->user);
    });



    connect(userWin,&userWidget::message,[=](QString m){
        m_snackbar->addMessage(m);
        if(m=="保存成功"){
            QString header = this->user.headerPic;
            if(!header.contains(":"))header = QCoreApplication::applicationDirPath()+header;
            m_avatar->setImage(QImage(header));
            userWin->freshUserDetails(this->user);
        }
    });


}

void MainWindow::buttonClick()
{
    QToolButton *b = (QToolButton *)sender();
    QString name = b->text();

    QList<QToolButton *> tbtns = ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *btn, tbtns) {
        btn->setChecked(btn == b);
    }

    if (name == "主界面") {
        ui->stackedWidget->setCurrentIndex(0);
    }else if (name == "系统设置") {
        ui->stackedWidget->setCurrentIndex(1);
        system->initSystem();
    } else if (name == "帮助与关于") {
        ui->stackedWidget->setCurrentIndex(2);
    } else if (name == "退出系统") {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "确认",
            "确定退出？",
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            exit(0);
        }

    }else if (name == "用户") {
        ui->stackedWidget->setCurrentIndex(3);
    }
}

void MainWindow::on_btnMenu_Min_clicked()
{
    showMinimized();
}

void MainWindow::on_btnMenu_Max_clicked()
{
    static bool max = false;
    static QRect location = this->geometry();

    if (max) {
        this->setGeometry(location);
    } else {
        location = this->geometry();
        this->setGeometry(QUIHelper::getScreenRect());
    }
    this->setProperty("canMove", max);
    max = !max;
}

void MainWindow::on_btnMenu_Close_clicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr,
        "确认",
        "确定退出？",
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        close();
    }
}


