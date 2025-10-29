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

#include "minimts_ex.h"

#include "hikcamera.h"

struct TestDetails{
    QString userName = "default";
    QString testNumber = "0000";
    QString testTime = "1970-1-1";

    QString testType = "strech";
    float length = 0.00;
    float speed = 0.00;

    float sampleSquare = 1;
    float sampleLength = 1;

    QString autoStopType = "Stop manually";
    float autoStopValue = 0;

    QString userFilePath = "";
    QString userFileName = "";

    bool DICDetectEnable = false;
    QString DICFileName = "DIC";
    QString DICFilePath = "";
    int DICDuration = 1000;
    QString DICTriggr = "inner";

    bool DICLENEnable = false;
    float pix = 1000;
    float trueLength = 10;
};

struct DataPoint {
    double timestamp;
    float x;
    float y;
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

    void initWidget();
    void initChart();
    void initPort();
    void initConnect();
    void initSystem();
    void addStateBox(QString message);
    void clearStateBox();

    MiniMTS_EX *minilogic = new MiniMTS_EX;
    HIKCamera hikCamera;
    QMap<QString, MV_CC_DEVICE_INFO*> devices;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    bool keyControlMoving = false;

public slots:
    void freshData();
    void limitStopOption(bool flag, int dir);

    void addState(QString msg);

    bool saveCsv(QList<QList<float>> data,QList<QString> header);

    bool saveCsv(QMap<QString, QLineSeries*> data);

    bool readCsv(QString path);

private:
    Ui::mainWidget *ui;

    int displayDir = 1;

    QTimer *dataFreshTimer = new QTimer(this);

    TestDetails testDetails;

    long zreoTime = 0;

    bool moveFlag = false;
    bool sampleProtectFlag = false;

    QString userHistroyPath;
    QMap<QString, QMap<QString, QString>> historyMap;


    std::deque<DataPoint> historyBufferA; // 历史光学引申数据缓存
    std::deque<DataPoint> historyBufferB;


private:
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



private slots:
    void leftMainClick();

    void on_btn_start_clicked();

    void on_btn_stop_clicked();

    void on_btn_emergence_clicked();

signals:
    void refresh();
    void connected(bool);
    void disconnected();
    void btnClicked(QString);
    void message(QString);

    void errOccured(QString type);

};

#endif // MAINWIDGET_H
