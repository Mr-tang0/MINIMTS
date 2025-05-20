#ifndef CHARTFORM_H
#define CHARTFORM_H

#include <QWidget>
#include <QRadioButton>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChart>
QT_CHARTS_USE_NAMESPACE

#include <QHBoxLayout>

#include <QDebug>


namespace Ui {
class chartForm;
}

enum AXIS{
    X_AXIS=1,
    Y_AXIS=2
};

class chartForm : public QWidget
{
    Q_OBJECT

public:
    explicit chartForm(QWidget *parent = nullptr);
    ~chartForm();

    void addSeries(QString SeriesTitle, QString Axis_Y_name, QString Axis_X_name);
    void addSeriesEx(QString mainTitle, QString subTitle);

    void addPoint(QString SeriesTitle, QPointF Point);
    void addPointEx(QString SeriesTitle, QString subTitle, QPointF Point);

    void addPoints(QString SeriesTitle, QList<QPointF> Points);
    void addPointsEx(QString SeriesTitle, QString subTitle, QList<QPointF> Points);

    void showSeries(QString SeriesTitle);

    void clearSeries(QString SeriesTitle);
    void clearSubSeries(QString SeriesTitle, QString subTitle);

    void clearAll();

    void setMeaaage(QString);

    void shortShow(int pointsCounts=100);
    void longShow();
    void saveAllPoints(const QString defualtName = "datafile");
    void showBar(bool flag = false);

    void getSeries(QString &SeriesTitle, QLineSeries *&ss);
    void getCurrentSeries(QString &SeriesTitle,QLineSeries *&ss);
    void getAllSeries(QMap<QString,QLineSeries*>* smap);

    void setSeriesPen(QString SeriesTitle, QPen pen);
    void setTitleFont(AXIS, QFont font);
    void setTickCount(AXIS, int TickCount =6);
    void setMinorTickCount(AXIS, int MinorTickCount=4);
    void setLabelsFont(AXIS, QFont font);
    void setLabelFormat(AXIS, int LabelFormat = 3);
    void setLabelsAngle(AXIS, int Angle =  -60);

    QMap<QString, QRadioButton*> buttonMap;

    void setLayoutAndChart(QChart *,QHBoxLayout *);

    void axisZoom(QString axisName = "x", float zoom =0, int deadNum=0){
        if(axisName.toUpper() == "X"){
            zoomX =zoom;
            deadNumX = deadNum;
        }else if(axisName.toUpper() == "Y"){
            zoomY =zoom;
            deadNumY = deadNum;
        }
    }

    void setSmooth(QString smoothtype, int windowSize){
        smoothWindow = windowSize;
        smoothType = smoothtype;
    }



private:
    Ui::chartForm *ui;

    QHBoxLayout *layout = new QHBoxLayout;
    QLineSeries *factSeries = new QLineSeries();
    QMap<QString, QLineSeries*> Series; //主曲线
    QMap<QString, QMap<QString, QLineSeries*>> SeriesEx;//伴随曲线

    QMap<QString, QStringList> nameMap;


    QChart *chart = new QChart();
    QValueAxis *xAxis = new QValueAxis;
    QValueAxis *yAxis = new QValueAxis;


    float zoomX =0;
    int deadNumX=0;
    float zoomY =0;
    int deadNumY=0;

    void updateAxisRanges(QLineSeries *series) {
        if (series->count() == 0) return;

        // 获取当前系列的数据范围
        qreal minX = series->at(0).x();
        qreal maxX = series->at(0).x();
        qreal minY = series->at(0).y();
        qreal maxY = series->at(0).y();

        for (int i = 1; i < series->count(); ++i) {
            const QPointF &point = series->at(i);
            minX = qMin(minX, point.x());
            maxX = qMax(maxX, point.x());
            minY = qMin(minY, point.y());
            maxY = qMax(maxY, point.y());
        }

        // 设置轴的范围
        if(zoomX==0)
            xAxis->setRange(minX-deadNumX, maxX+deadNumX);
        else
            xAxis->setRange(int(minX-minX*zoomX)-deadNumX, int(maxX+maxX*zoomX)+deadNumX);

        if(zoomY ==0)
            yAxis->setRange(minY-deadNumY, maxY+deadNumY);
        else{
            if(minY<=0)
                yAxis->setRange(int(minY+minY*zoomY)-deadNumY, int(maxY+maxY*zoomY)+deadNumY);
            else
                yAxis->setRange(int(minY-minY*-zoomY)-deadNumY, int(maxY+maxY*zoomY)+deadNumY);
        }
    }

    int showCounts = 8000;

    void showSeries_private(QString SeriesTitle);

    int smoothWindow = 1;
    QString smoothType = "";

};

#endif // CHARTFORM_H
