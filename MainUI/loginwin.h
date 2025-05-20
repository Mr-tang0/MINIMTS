#ifndef LOGINWIN_H
#define LOGINWIN_H

#include"tool.h"
#include <QWidget>
#include "qtmaterialsnackbar.h"
#include <QPainter>
#include <QKeyEvent>

class QtMaterialAvatar;
class QtMaterialBadge;

namespace Ui {
class loginWin;
}

class loginWin : public QWidget
{
    Q_OBJECT

public:
    explicit loginWin(QWidget *parent = nullptr);
    ~loginWin();

    void setHeader(QString path);
    void setCurrentUser(User user);

    static QMap<QString, QMap<QString, QString>> usersMap;

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
            onEnterPressed();
        }
        QWidget::keyPressEvent(event);
    }


private slots:
    void on_loginChecked_2_clicked();

    void on_loginChecked_clicked();

    void createImageWithInitial(const QString &userName, const QString &path);

    void onEnterPressed();


private:
    Ui::loginWin *ui;
    QtMaterialSnackbar    *const m_snackbar;
    QtMaterialAvatar      *const m_avatar;
    QtMaterialBadge       *const m_badge;



    QString rootPath ;

signals:
    void logined(User);

};

#endif // LOGINWIN_H
