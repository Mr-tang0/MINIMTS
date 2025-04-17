#ifndef LOGINWIN_H
#define LOGINWIN_H

#include"tool.h"
#include <QWidget>
#include "qtmaterialsnackbar.h"
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

private slots:
    void on_loginChecked_2_clicked();

    void on_loginChecked_clicked();

    void createImageWithInitial(const QString &userName, const QString &path);


private:
    Ui::loginWin *ui;
    QtMaterialSnackbar    *const m_snackbar;
    QtMaterialAvatar      *const m_avatar;
    QtMaterialBadge       *const m_badge;

signals:
    void logined(User);

};

#endif // LOGINWIN_H
