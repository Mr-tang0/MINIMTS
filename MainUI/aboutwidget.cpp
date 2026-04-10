#include "aboutwidget.h"
#include "ui_aboutwidget.h"
#include <QFile>
#include <QCommandLineParser>
// #include "mainwindow.h"

aboutWidget::aboutWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::aboutWidget)
{
    ui->setupUi(this);

    // --- 1. 初始化 Help 页面 ---
    // 不要指定父对象，由布局接管
    QWebEngineView *helpView = new QWebEngineView();
    QString helpPath = QCoreApplication::applicationDirPath() + "/html/help.html";
    helpView->load(QUrl::fromLocalFile(helpPath));

    // 检查 UI 控件是否已有布局，防止重复创建布局导致崩溃
    if (!ui->helpView->layout()) {
        QVBoxLayout *helpLayout = new QVBoxLayout(ui->helpView);
        helpLayout->setContentsMargins(0, 0, 0, 0);
        helpLayout->addWidget(helpView);
    }

    // --- 2. 初始化 About 页面 ---
    QWebEngineView *aboutView = new QWebEngineView();
    QString aboutPath = QCoreApplication::applicationDirPath() + "/html/about.html";
    aboutView->load(QUrl::fromLocalFile(aboutPath));

    if (!ui->adoutView->layout()) {
        QVBoxLayout *aboutLayout = new QVBoxLayout(ui->adoutView);
        aboutLayout->setContentsMargins(0, 0, 0, 0);
        aboutLayout->addWidget(aboutView);
    }
}

aboutWidget::~aboutWidget()
{
    delete ui;
}

void aboutWidget::initThis(){

}


