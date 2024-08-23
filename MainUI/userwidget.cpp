#include "userwidget.h"
#include "ui_userwidget.h"
#include <qtmaterialavatar.h>
#include <qtmaterialbadge.h>
#include "mainwindow.h"

userWidget::userWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::userWidget),
    m_avatar(new QtMaterialAvatar()),
    m_badge(new QtMaterialBadge)
{
    ui->setupUi(this);

    m_avatar->setImage(QImage(":/main/res/icon/sikh.jpg"));
    m_avatar->setSize(200);

    QLayout *layout = ui->userHeader->layout();
    if (layout == nullptr) {
        layout = new QVBoxLayout(ui->userHeader);
    }
    layout->addWidget(m_avatar);


    QFileSystemModel *model = new QFileSystemModel(this);
    model->setRootPath(QDir::currentPath()); // 设置初始根目录

    // 设置模型到QTreeView
    ui->file_treeView->setModel(model);
    ui->file_treeView->setRootIndex(model->index(QDir::currentPath()));


}

userWidget::~userWidget()
{
    delete ui;
}

void userWidget::freshUserDetails(User user){

    QString header = user.headerPic;
    if(!header.contains(":"))header = QCoreApplication::applicationDirPath()+header;

    m_avatar->setImage(QImage(header));

    ui->userName->setText(user.name);
    ui->userID->setText(user.userid);
    ui->defaultFileName->setText(user.userDefaultName);
    ui->defaultSavePath->setText(user.userfilePath);
    ui->password->setText(user.password);
    ui->userForce->setText(user.role);
    ui->lastLogin->setText(user.lastTestTime);

    QString Directory = user.userfilePath;
    if(!Directory.contains(":"))Directory = QCoreApplication::applicationDirPath()+Directory;
    ui->lab_filePath->setText(Directory);

    if (!Directory.isEmpty()) {
        QFileSystemModel *model = static_cast<QFileSystemModel*>(ui->file_treeView->model());
        ui->file_treeView->setRootIndex(model->index(Directory));
    }

}

void userWidget::chooseHeader(){
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open Image"), "", tr("Image Files (*.png *.jpg *.bmp)"));

    if (!filePath.isEmpty()) {
        tempHeaderPath = filePath;
    }

}

void userWidget::on_pushButton_clicked()
{
    MainWindow::user.name=ui->userName->text();
    MainWindow::user.password = ui->password->text();
    MainWindow::user.userfilePath = ui->defaultSavePath->text();
    MainWindow::user.userDefaultName = ui->defaultFileName->text();

    QString targetPath = QCoreApplication::applicationDirPath()+"/root/users/"+MainWindow::user.name+"/";

    QString fileName = QFileInfo(tempHeaderPath).fileName();
    QString targetFilePath = targetPath + MainWindow::user.name+fileName;

    QDir dir(targetPath);
    if (!dir.exists()) {
        if (!dir.mkpath(targetPath)) {
            qDebug() << "Failed to create directory:" << targetPath;
            return;
        }
    }

    if (QFile::copy(tempHeaderPath, targetFilePath)) {
        MainWindow::user.headerPic = targetFilePath;
    }

    Tool::freshUser(MainWindow::user,&(MainWindow::usersMap));


    emit message("保存成功");
}

void userWidget::on_headerChoose_clicked()
{
    chooseHeader();

}

void userWidget::on_pushButton_2_clicked()
{

    QString home = MainWindow::user.userfilePath;
    QString filePath = QFileDialog::getExistingDirectory(this, tr("Select Directory"), home);
    if (!filePath.isEmpty()) {
        ui->defaultSavePath->setText(filePath);
    }
}
