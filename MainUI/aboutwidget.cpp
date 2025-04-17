#include "aboutwidget.h"
#include "ui_aboutwidget.h"
#include <QFile>
#include <QCommandLineParser>
#include "mainwindow.h"

aboutWidget::aboutWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::aboutWidget)
{
    ui->setupUi(this);

    // initThis();
}

aboutWidget::~aboutWidget()
{
    delete ui;
}

// void aboutWidget::initThis(){
//     QVBoxLayout *helpLayout = new QVBoxLayout;
//     helpLayout->addWidget(helpEngineView);
//     ui->helpView->setLayout(helpLayout);

//     QVBoxLayout *aboutLayout = new QVBoxLayout;
//     aboutLayout->addWidget(aboutEngineView);
//     ui->adoutView->setLayout(aboutLayout);


//     QCommandLineParser parser;
//     parser.setApplicationDescription("QtWebEngine Example");
//     parser.addHelpOption();
//     parser.addVersionOption();
//     parser.addPositionalArgument("url", "The initial URL to open.");
//     parser.process(QCoreApplication::arguments());

//     QString root = QCoreApplication::applicationDirPath();
//     QString helpDocPath = MainWindow::rootMap["system"]["helpDocPath"];
//     QString requesturl = MainWindow::rootMap["system"]["requesturl"];
//     QString appurl = MainWindow::rootMap["system"]["appurl"];

//     QString initialUrl;
//     if (MainWindow::rootMap["system"]["localHelp"]=="yes"){
//         initialUrl = parser.positionalArguments().isEmpty() ?
//                          root+helpDocPath : parser.positionalArguments().first();
//     }else{
//         initialUrl = parser.positionalArguments().isEmpty() ?
//                          helpDocPath : parser.positionalArguments().first();
//     }

//     helpEngineView->load(QUrl(initialUrl));
//     appurl = "https://www.baidu.com";

//     initialUrl = parser.positionalArguments().isEmpty() ?
//                      appurl : parser.positionalArguments().first();

//     aboutEngineView->loadUrl(initialUrl);


//     ui->addressEdit->setText(initialUrl);

//     connect(aboutEngineView,&myWebEngineView::linkClicked,[=](QUrl url){
//         ui->addressEdit->setText(url.toString());
//     });

// }

// void aboutWidget::on_goto_btn_clicked()
// {
//     QString url = ui->addressEdit->text();

//     aboutEngineView->clearMask();
//     aboutEngineView->loadUrl(url);

// }


// void aboutWidget::on_back_clicked()
// {
//     aboutEngineView->toBack();
// }

// void aboutWidget::on_forward_clicked()
// {
//     aboutEngineView->toForward();
// }

