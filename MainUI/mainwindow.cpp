#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <qtmaterialavatar.h>
#include <qtmaterialbadge.h>
#include "iconhelper.h"
#include "quihelper.h"
#include "xmlreader.h"


// 静态成员初始化
MINIMTSUser *MainWindow::user = new MINIMTSUser;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_snackbar(new QtMaterialSnackbar)
    , m_avatar(new QtMaterialAvatar(QImage(":/main/res/icon/user.png")))
    , m_badge(new QtMaterialBadge)
{
    ui->setupUi(this);

    // 1. 初始化 Material 风格组件
    m_snackbar->setParent(this);
    m_snackbar->setAutoHideDuration(400);
    m_snackbar->setFont(QFont("幼圆"));
    m_snackbar->setFontSize(12);

    m_avatar->setSize(60);
    m_badge->setParent(m_avatar);
    m_badge->setRelativePosition(18, 12);
    m_badge->setText("0");
    m_badge->setEnabled(false);

    // 2. 将头像添加到布局
    QLayout *headerLayout = ui->header->layout();
    if (headerLayout == nullptr) {
        headerLayout = new QVBoxLayout(ui->header);
    }
    headerLayout->addWidget(m_avatar);

    // 3. 基础界面与子窗口初始化
    initThis();
    initStyle();
    initConnections();


    // 4. 初始状态设置
    if(system->systemMap["advance"]["autoFindVersion"] == "是") {
        runUpdateCommand();
    }

    onlinelogin->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// -------------------------------------------------------------------------
// 初始化逻辑
// -------------------------------------------------------------------------

void MainWindow::initThis()
{
    QUIHelper::setFramelessForm(this);
    this->centralWidget()->layout()->setMargin(1);

    // 设置顶部菜单图标
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

    // 实例化子页面
    system      = new systemWidget;
    mainwd      = new mainWidget;
    onlinelogin = new OnlineLogin;
    about       = new aboutWidget;
    userWin     = new userWidget;

    mainwd->initSystem();

    // 将子页面添加到 StackedWidget 对应的 Page 中
    QList<QWidget*> widgetList;
    widgetList << mainwd << system << about << userWin;

    for (int i = 0; i < widgetList.length(); i++) {
        QWidget *page = ui->stackedWidget->widget(i);
        QLayout *layout = page->layout();
        if (layout == nullptr) {
            layout = new QVBoxLayout(page);
        }
        layout->addWidget(widgetList[i]);
        page->update();
    }
}

void MainWindow::initConnections()
{
    // 限位传感器逻辑处理
    connect(mainwd->minilogic, &MiniMTSLogic::LimitOccured, this, [=](int axis){
        if (axis != oldAxis) {
            oldAxis = axis;
            if (axis != 0) {
                addErrMessage(QStringLiteral("限位传感器被触发:方向 %1").arg(axis));
                mainwd->limitStopOption(true, axis);
            } else {
                deleteErrMessage(QStringLiteral("限位传感器被触发:方向 %1").arg(axis));
                mainwd->limitStopOption(false, axis);
            }
        }
    });

    // 核心业务信号
    connect(mainwd, &mainWidget::errOccured, this, [=](QString err){ addErrMessage(err); });
    connect(mainwd, &mainWidget::connected, this, [=](bool flag){
        m_snackbar->addMessage(flag ? "连接成功！" : "连接失败！");
    });
    connect(mainwd, &mainWidget::disconnected, this, [=](){ m_snackbar->addMessage("已断开！"); });
    connect(mainwd, &mainWidget::message, this, [=](QString message){ m_snackbar->addMessage(message); });
    connect(mainwd, &mainWidget::btnClicked, this, [=](QString btn){
        if(btn == "toSet") ui->btnConfig->click();
    });

    // 系统设置信号
    connect(system, &systemWidget::saved, this, [=](bool flag){
        if(flag) {
            m_snackbar->addMessage("保存成功！");
            mainwd->initSystem();
        }
        ui->btnMain->click();
    });
    connect(system, &systemWidget::openPIDContorl, this, [=](){
        mainwd->minilogic->openPIDControl(systemWidget::systemMap["normal"]);
    });

    // 用户与登录信号
    connect(user, &MINIMTSUser::logined, user, &MINIMTSUser::getOnlineDetailToLocal);
    connect(user, &MINIMTSUser::logined, this, [=](){
        this->show();
        user->creatLocal();
        onlinelogin->setCurrentUser(user);
        onlinelogin->hide();


        userWin->freshUserDetails(user);
        m_avatar->setImage(QImage(user->header));
    });

    //注册后初始化用户
    connect(user, &MINIMTSUser::registered, this, [=](){
        mainWidget::historyMap.clear();

        QMap<QString, QMap<QString, QString>> temp;
        temp["project"] = mainWidget::historyMap;
        XMLReader::Save_XML_File(temp, mainwd->userHistroyPath);
        user->updateLocalDetailToCloud();
    });


    connect(userWin, &userWidget::WanaExit, this, [=](){
        this->close();
        onlinelogin->show();
        onlinelogin->setCurrentUser(user);
        m_snackbar->addMessage("退出成功");
    });
}

void MainWindow::initStyle()
{
    // 统一配置常用控件微调 QSS
    int addWidth = 20, addHeight = 10, rbtnWidth = 15, ckWidth = 13, scrWidth = 12, borderWidth = 3;
    QStringList qss;
    qss << QString("QComboBox::drop-down,QDateEdit::drop-down,QTimeEdit::drop-down,QDateTimeEdit::drop-down{width:%1px;}").arg(addWidth);
    qss << QString("QComboBox::down-arrow,QDateEdit[calendarPopup=\"true\"]::down-arrow,QTimeEdit[calendarPopup=\"true\"]::down-arrow,QDateTimeEdit[calendarPopup=\"true\"]::down-arrow{width:%1px;height:%1px;right:2px;}").arg(addHeight);
    qss << QString("QRadioButton::indicator{width:%1px;height:%1px;}").arg(rbtnWidth);
    qss << QString("QCheckBox::indicator,QGroupBox::indicator,QTreeWidget::indicator,QListWidget::indicator{width:%1px;height:%1px;}").arg(ckWidth);
    qss << QString("QScrollBar:horizontal{min-height:%1px;border-radius:%2px;}QScrollBar::handle:horizontal{border-radius:%2px;}QScrollBar:vertical{min-width:%1px;border-radius:%2px;}QScrollBar::handle:vertical{border-radius:%2px;}").arg(scrWidth).arg(scrWidth / 2);
    qss << QString("QWidget#widget_top>QToolButton:pressed,QWidget#widget_top>QToolButton:hover,QWidget#widget_top>QToolButton:checked,QWidget#widget_top>QLabel:hover{border-width:0px 0px %1px 0px;}").arg(borderWidth);
    qss << QString("QWidget#widgetleft>QPushButton:checked,QWidget#widgetleft>QToolButton:checked,QWidget#widgetleft>QPushButton:pressed,QWidget#widgetleft>QToolButton:pressed{border-width:0px 0px 0px %1px;}").arg(borderWidth);

    this->setStyleSheet(qss.join(""));

    // 初始化导航按钮状态
    QList<QToolButton *> tbtns = ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *btn, tbtns) {
        btn->setIconSize(QSize(32, 32));
        btn->setMinimumWidth(85);
        btn->setCheckable(true);
        connect(btn, SIGNAL(clicked()), this, SLOT(buttonClick()));
    }
    ui->btnMain->click();
}

// -------------------------------------------------------------------------
// 业务功能实现
// -------------------------------------------------------------------------

void MainWindow::runUpdateCommand()
{
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString thisID = QString::number(VersionID);
    qDebug() << "Checking updates, current ID: " << thisID;

    QString program = appDirPath + "/update";
    QStringList arguments;
    arguments << "https://raw.gitcode.com/m0_51547074/update_info/raw/main/minimtsinfo.json"
              << "MINIMTS"
              << thisID;

    if (!QProcess::startDetached(program, arguments, appDirPath)) {
        qDebug() << "Failed to start update program.";
    } else {
        qDebug() << "Update command started successfully.";
    }
}

void MainWindow::addErrMessage(QString msg)
{
    int errCount = m_badge->text().toInt() + 1;
    m_badge->setText(QString::number(errCount));
    m_badge->setEnabled(true);
}

void MainWindow::deleteErrMessage(QString)
{
    int errCount = m_badge->text().toInt();
    if (errCount > 1) {
        m_badge->setText(QString::number(errCount - 1));
        m_badge->setEnabled(true);
    } else {
        m_badge->setText("0");
        m_badge->setEnabled(false);
    }
}

// -------------------------------------------------------------------------
// 事件处理
// -------------------------------------------------------------------------

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(!filteropen) return false;

    // 双击标题栏最大化
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == ui->widgetTitle) {
            on_btnMenu_Max_clicked();
            return true;
        }
    }

    // 鼠标拖动与边缘检测逻辑
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        const int topMargin = 5;
        bool isNearTopEdge = (mouseEvent->globalY() <= topMargin);

        if (isNearTopEdge && windowState() != Qt::WindowMaximized) {
            ui->btnMenu_Max->click();
            showMaximized();
            filteropen = false;
            Tool::delay(500);
            filteropen = true;
        } else if (!isNearTopEdge && windowState() == Qt::WindowMaximized) {
            ui->btnMenu_Max->click();
            filteropen = false;
            Tool::delay(500);
            filteropen = true;
        }
        lastPos = mouseEvent->globalPos();
    }
    return QWidget::eventFilter(watched, event);
}

void MainWindow::buttonClick()
{
    QToolButton *b = (QToolButton *)sender();
    QString name = b->text();

    // 切换按钮选中状态
    QList<QToolButton *> tbtns = ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *btn, tbtns) {
        btn->setChecked(btn == b);
    }

    // 页面切换逻辑
    if (name == "主界面") {
        ui->stackedWidget->setCurrentIndex(0);
    } else if (name == "系统设置") {
        ui->stackedWidget->setCurrentIndex(1);
        system->initSystem();
    } else if (name == "帮助与关于") {
        ui->stackedWidget->setCurrentIndex(2);
    } else if (name == "退出系统") {
        on_btnMenu_Close_clicked();
    } else if (name == "用户") {
        ui->stackedWidget->setCurrentIndex(3);
    } else if (name == "DIC分析") {
        QString dicExePath = QCoreApplication::applicationDirPath() + "/dic.exe";
        QFileInfo fileInfo(dicExePath);

        if (fileInfo.exists() && fileInfo.isExecutable()) {
            QProcess *process = new QProcess(this);
            process->start(dicExePath);
            m_snackbar->addMessage("DIC分析程序初始化中，请稍后！");

            if (!process->waitForStarted(3000)) {
                m_snackbar->addMessage("DIC分析程序启动失败！");
                process->deleteLater();
            } else {
                connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        process, &QObject::deleteLater);
            }
        } else {
            m_snackbar->addMessage("暂不可用！");
        }
    }
}

// -------------------------------------------------------------------------
// 窗口控制按钮槽函数
// -------------------------------------------------------------------------

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
        nullptr, "确认", "确定退出？", QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        qDebug()<<"备份软件中";
        user->updateLocalDetailToCloud();
        Tool::delay(200);

        if(mainwd->hikWin.camera->isOpened()) {
            mainwd->hikWin.camera->Close();
        }

        exit(0);
    }
}
