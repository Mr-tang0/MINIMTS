#ifndef ABOUTWIDGET_H
#define ABOUTWIDGET_H

#include <QWidget>
// #include "mywebengineview.h"

namespace Ui {
class aboutWidget;
}

class aboutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit aboutWidget(QWidget *parent = nullptr);
    ~aboutWidget();
    void initThis();

private slots:
    // void on_goto_btn_clicked();

    // void on_back_clicked();

    // void on_forward_clicked();

private:
    Ui::aboutWidget *ui;
    // myWebEngineView *helpEngineView = new myWebEngineView;
    // myWebEngineView *aboutEngineView = new myWebEngineView;

    QStringList urlList;
    QString CurrentUrl;
};

#endif // ABOUTWIDGET_H
