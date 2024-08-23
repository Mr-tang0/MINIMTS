#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>
#include <QTimer>

QT_CHARTS_USE_NAMESPACE
#include "iconhelper.h"
#include "quihelper.h"
#include <QAbstractButton>
#include "mgmotorlogic.h"

struct TestDetails{
    QString userName = "default";
    QString testNumber = "0000";
    QString testTime = "1970-1-1";

    QString testType = "strech";
    float length = 0.00;
    float speed = 0.00;

    float sampleSquare = 1;
    float sampleLength = 1;

    QString autoStopType = "手动停止";
    float autoStopValue = 0;

    QString userFilePath = "";
    QString userFileName = "";
};


namespace Ui {
class mainWidget;
}

class mainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit mainWidget(QWidget *parent = nullptr);
    ~mainWidget();
    void freshUi();
    void freshData();
    void clearChart();
    void clearChannal();
    void initSystem();
    void addStateBox(QString message);
    void clearStateBox();

    void initPortWin();
    void initPorjectWin();


private:
    Ui::mainWidget *ui;
    TestDetails testDetails;
    QTimer *uiFreshTimer = new QTimer(this);
    QTimer *dataFreshTimer = new QTimer(this);

    long zreoTime = 0;
    float zreoForce = 0;
    float zreoLength = 0;
    float zreoFactLength = 0;

    QList<float> currentValueList = {0,0,0,0,0,0};

    void initWidget();
    void initChart();
    void initPort();
    void initConnect();


    QLineSeries *factSeries = new QLineSeries();
    int currentLineSeriesID = 0;

    QList<QLineSeries*> lineSeriesList;
    QList<QList<float>> axisPointsList;

    QList<QList<float>> maxAndMinPointsList={{1,0},{1,0},{1,0},{1,0},{1,0},{1,0}};

    QChart *chart = new QChart();

    QValueAxis *chartXAxis = new QValueAxis;
    QValueAxis *chartYAxis = new QValueAxis;

    bool startTestFlag = false;

private:
    //根据QSS样式获取对应颜色值
    QList<QToolButton *> btnsMain;
    bool motorConnectFlag = false;

    QString borderColor;
    QString normalBgColor;
    QString darkBgColor;
    QString normalTextColor;
    QString darkTextColor;

    static void getQssColor(const QString &qss, const QString &flag, QString &color);
    static void getQssColor(const QString &qss, QString &textColor,
                     QString &panelColor, QString &borderColor,
                     QString &normalColorStart, QString &normalColorEnd,
                     QString &darkColorStart, QString &darkColorEnd,
                     QString &highColor);

    MGMotorLogic *logic = new MGMotorLogic;

private slots:
    void leftMainClick();

    void on_btn_start_clicked();

    void on_btn_stop_clicked();

signals:
    void refresh();
    void connected(bool);
    void disconnected();
    void btnClicked(QString);
    void massage(QString);

};

#endif // MAINWIDGET_H
