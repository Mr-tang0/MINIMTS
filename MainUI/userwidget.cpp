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

    setupContextMenu();

}

userWidget::~userWidget()
{
    delete ui;
}
bool userWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->file_treeView)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            qDebug()<<"clicked";
//            if (event->buttons() & Qt::LeftButton)
//            {
//                QModelIndex index = ui->file_treeView->indexAt(event->pos());
//                if (!index.isValid())
//                {
//                    ui->file_treeView->setCurrentIndex(QModelIndex());
//                }
//            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void userWidget::setupContextMenu() {
    QFileSystemModel *model = new QFileSystemModel(this);
    model->setRootPath(QDir::currentPath());


    ui->file_treeView->setModel(model);
    ui->file_treeView->setRootIndex(model->index(QDir::currentPath()));
    ui->file_treeView->expandAll();
    ui->file_treeView->setContextMenuPolicy(Qt::CustomContextMenu);



    connect(ui->file_treeView, &QTreeView::doubleClicked, [=]() {
        QModelIndex index = ui->file_treeView->currentIndex();
        if (index.isValid()) {
            QString filePath = model->filePath(index);
            if (!model->isDir(index)) {
                QFileInfo fileInfo(filePath);
                QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
            }
        }

    });



    QMenu *menu = new QMenu(ui->file_treeView);
    menu->addAction("新建文件夹");
    menu->addAction("复制");
    menu->addAction("粘贴");
    menu->addAction("删除");

    connect(ui->file_treeView, &QTreeView::customContextMenuRequested, this, [=](QPoint pos) {
        QPoint globalPos = ui->file_treeView->mapToGlobal(pos);
        menu->exec(globalPos);

    });

    connect(menu,&QMenu::triggered,[=](QAction *action){
        QString trd = action->text();
        if(trd=="新建文件夹"){
            QModelIndex index = ui->file_treeView->currentIndex();
            QString parentPath = model->filePath(index);  // 获取当前选中索引的路径
            QFileInfo fileInfo(parentPath);

            if (fileInfo.isDir()) {
                bool ok;
                QString folderName = QInputDialog::getText(this, "新建文件夹", "请输入文件夹名称:", QLineEdit::Normal, "", &ok);

                if (ok && !folderName.isEmpty()) {
                    QString newPath = parentPath + "/" + folderName;
                    bool created = QDir().mkpath(newPath);

                    if (created) {
                        ui->file_treeView->setModel(model);
                    } else {
                        qDebug() << "文件夹创建失败：" << newPath;
                    }
                }
            }
        }else if(trd=="复制"){
            QModelIndex index = ui->file_treeView->currentIndex();
            QString path = model->filePath(index);  // 获取当前选中索引的路径
            QFileInfo fileInfo(path);

            if (fileInfo.exists()) {  // 检查路径是否有效
                if (fileInfo.isFile()) {  // 如果是文件
                    Tool::copyPathToClipboard(path);
                } else if (fileInfo.isDir()) {  // 如果是目录
                    Tool::copyPathToClipboard(path);
                }
            } else {
                qDebug() << "无效的路径：" << path;
            }

        }else if(trd=="粘贴"){
            // 获取当前选中的索引
            QModelIndex index = ui->file_treeView->currentIndex();

            // 获取父目录的路径
            QString parentPath = model->filePath(index);

            // 从剪贴板获取文件路径
            QString filePath = QApplication::clipboard()->text();

            // 获取文件名和完整路径
            QFileInfo fileInfo(filePath);
            QString fileName = fileInfo.fileName();
            QString sourcePath = fileInfo.absoluteFilePath();

            // 构建目标路径
            QString destPath = parentPath + "/" + fileName;

            // 检查是否为文件
            if (fileInfo.isFile()) {
                QFile::copy(sourcePath, destPath);
            } else if (fileInfo.isDir()) {
                QDir().mkpath(destPath);
            }
        }else if(trd=="删除"){
            QModelIndex index = ui->file_treeView->currentIndex();
            QString path = model->filePath(index);
            QFileInfo fileInfo(path);
            if (fileInfo.exists()&&fileInfo.isDir()) {
                QMessageBox::StandardButton reply = QMessageBox::question(this, tr("确认删除"),
                                                                          tr("您确定要删除 \"%1\" 及其所有内容吗？").arg(fileInfo.fileName()),
                                                                          QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    bool removed = Tool::removeDirectoryRecursively(path);
                    if(!removed)qDebug() << "失败" << path;
                }
            } else {
                bool removed = QFile::remove(path);
                if(!removed)qDebug() << "无效的路径：" << path;
            }
            emit message("删除成功");
        }
    });

    connect(ui->copyPath,&QPushButton::clicked,[=](){
        QApplication::clipboard()->setText(ui->lab_filePath->text());
        emit message("复制成功");
    });


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
    tempHeaderPath = user.headerPic;

    QString Directory = user.userfilePath;
    if(!Directory.contains(":"))Directory = QCoreApplication::applicationDirPath()+Directory;
    ui->lab_filePath->setText(Directory);

    if (!Directory.isEmpty()) {
        QFileSystemModel *model = static_cast<QFileSystemModel*>(ui->file_treeView->model());
        ui->file_treeView->setRootIndex(model->index(Directory));
        ui->file_treeView->setColumnWidth(0,400);
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
    MainWindow::user.userid = ui->userID->text();

    QString targetPath = QCoreApplication::applicationDirPath()+"/root/users/"+MainWindow::user.name+"/";

    QString fileName = QFileInfo(tempHeaderPath).fileName();
    QString targetFilePath = targetPath +"user/"+MainWindow::user.name+fileName;

    QDir ndir(targetPath +"user/");
    if (!ndir.exists()) {
        if (!ndir.mkpath(targetPath +"user/")) {
            return;
        }
    }

    QFile::copy(tempHeaderPath, targetFilePath);
    MainWindow::user.headerPic = targetFilePath;

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

void userWidget::on_exitUser_clicked()
{
    emit WanaExit();
}

