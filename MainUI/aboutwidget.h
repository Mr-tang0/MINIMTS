#ifndef ABOUTWIDGET_H
#define ABOUTWIDGET_H

#include <QWidget>
#include "pdfview.h"

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

private:
    Ui::aboutWidget *ui;
    PDFView view;

    QStringList urlList;
    QString CurrentUrl;
};

#endif // ABOUTWIDGET_H
