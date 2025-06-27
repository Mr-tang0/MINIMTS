#ifndef SYSTEMWIDGET_H
#define SYSTEMWIDGET_H

#include <QWidget>
#include <QMap>
#include <QToolButton>

namespace Ui {
class systemWidget;
}

class systemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit systemWidget(QWidget *parent = nullptr);
    ~systemWidget();

    void initSystem();

    void normolSaveSystem();
    void advanceSaveSystem();
    void dicSaveSystem();

    static QMap<QString, QMap<QString, QString>> systemMap;
    QList<QToolButton *> btnsMain;

private slots:
    void leftMainClick();
    void on_readSet_clicked();

    void on_saveSetAs_clicked();

private:
    Ui::systemWidget *ui;

    QString rootPath;

signals:
    void saved(bool);
    void setupCamera();
};

#endif // SYSTEMWIDGET_H
