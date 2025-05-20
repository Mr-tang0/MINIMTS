#include "chartform.h"
#include "ui_chartform.h"
#include <QDebug>
#include <QToolTip>
#include <QFileDialog>

#include <QtConcurrent/QtConcurrent>


chartForm::chartForm(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::chartForm)
{
    ui->setupUi(this);
    chart->legend()->setVisible(false);

    QFont font;
    font.setFamily("Arial");
    font.setPointSize(12);

    xAxis->setTitleText("X");//标题
    xAxis->setTitleFont(font);
    xAxis->setTickCount(6); //主分隔个数：0到10分成20个单位
    xAxis->setMinorTickCount(4); //每个单位之间绘制了多少虚网线
    xAxis->setLabelsFont(font);
    xAxis->setLabelFormat("%.3f"); //标签格式：每个单位保留几位小数

    yAxis->setTitleText("Y");//标题
    yAxis->setTitleFont(font);
    yAxis->setTickCount(6); //主分隔个数：0到10分成20个单位
    yAxis->setMinorTickCount(4); //每个单位之间绘制了多少虚网线
    yAxis->setLabelsFont(font);
    yAxis->setLabelFormat("%.3f"); //标签格式：每个单位保留几位小数


    setLayoutAndChart(chart, layout);
    showBar(false);
}

chartForm::~chartForm()
{
    delete ui;
}

void chartForm::setLayoutAndChart(QChart * c,QHBoxLayout * l){
    ui->chartView->setChart(c);

    if(ui->buttonWidget->layout())
        delete ui->buttonWidget->layout();

    ui->buttonWidget->setLayout(l);

}

void chartForm::setTitleFont(AXIS ss, QFont font){
    if(ss==X_AXIS)
        xAxis->setTitleFont(font);
    else
        yAxis->setTitleFont(font);
}

void chartForm::setTickCount(AXIS ss, int TickCount){
    if(ss==X_AXIS)
        xAxis->setTickCount(TickCount);
    else
        yAxis->setTickCount(TickCount);
}

void chartForm::setMinorTickCount(AXIS ss, int MinorTickCount){
    if(ss==X_AXIS)
        xAxis->setMinorTickCount(MinorTickCount);
    else
        yAxis->setMinorTickCount(MinorTickCount);
}
void chartForm::setLabelsFont(AXIS ss, QFont font){
    if(ss==X_AXIS)
        xAxis->setLabelsFont(font);
    else
        yAxis->setLabelsFont(font);
}
void chartForm::setLabelFormat(AXIS ss, int LabelFormat){
    if(ss==X_AXIS)
        xAxis->setLabelFormat(QStringLiteral("%.%1f").arg(LabelFormat));
    else
        yAxis->setLabelFormat(QStringLiteral("%.%1f").arg(LabelFormat));
}
void chartForm::setLabelsAngle(AXIS ss, int Angle){
    if(ss==X_AXIS)
        xAxis->setLabelsAngle(-60);
    else
        yAxis->setLabelsAngle(Angle);
}

void chartForm::addSeries(QString s, QString axisY, QString axisX){
    //防止曲线名冲突
    int i=0;
    QString temp = s;
    while(Series.keys().contains(temp)){
        i++;
        temp=s+QStringLiteral("(%1)").arg(i);
    }
    s = temp;

    //添加曲线
    Series[s] = new QLineSeries;
    QStringList name = {axisY,axisX};
    nameMap[s] = name;
    buttonMap[s] = new QRadioButton;
    buttonMap[s]->setText(s);
    layout->addWidget(buttonMap[s]);

    connect(buttonMap[s],&QRadioButton::toggled,this,[=](bool f){
        showSeries_private(s);
    });
    chart->addSeries(Series[s]);
    Series[s]->setName(s);
    chart->setAxisX(xAxis, Series[s]);
    chart->setAxisY(yAxis, Series[s]);

    connect(Series[s], &QLineSeries::hovered, [=](const QPointF &point, bool state)mutable{
        if (state) QToolTip::showText(QCursor::pos(), QString("(%1,%2)").arg(point.x()).arg(point.y()));
    });

    //显示一条曲线
    showSeries_private(s);

    //图标中有多条曲线，打开曲线切换选项
    if(Series.count()>1)
        showBar(true);

}

void chartForm::addSeriesEx(QString mainTitle, QString subTitle){
    //防止曲线名冲突
    int i=0;
    QString temp = subTitle;
    while(Series.keys().contains(temp)){
        i++;
        temp=subTitle+QStringLiteral("(%1)").arg(i);
    }
    subTitle = temp;

    SeriesEx[mainTitle][subTitle] = new QLineSeries;
    SeriesEx[mainTitle][subTitle]->setName(subTitle);

    chart->addSeries(SeriesEx[mainTitle][subTitle]);
    chart->setAxisX(xAxis, SeriesEx[mainTitle][subTitle]);
    chart->setAxisY(yAxis, SeriesEx[mainTitle][subTitle]);

    connect(SeriesEx[mainTitle][subTitle], &QLineSeries::hovered, [=,&subTitle](const QPointF &point, bool state)mutable{
        if (state) QToolTip::showText(QCursor::pos(), QString("(%1,%2)").arg(point.x()).arg(point.y()));
    });

    showSeries_private(mainTitle);
}


void chartForm::setSeriesPen(QString SeriesTitle, QPen pen){
    Series[SeriesTitle]->setPen(pen);
}

void chartForm::showSeries(QString s){
    if(!buttonMap[s]->isChecked()){
        buttonMap[s]->click();
    }
}

void chartForm::showSeries_private(QString s){

    if(!chart->series().isEmpty())
    {
        for(int i = 0;i<chart->series().length();i++)
            chart->series()[i]->setVisible(false);
    }

    Series[s]->setVisible(true);


    if(!SeriesEx[s].keys().isEmpty()){
        chart->legend()->setVisible(true); // 确保图例可见
        foreach (QString key, SeriesEx[s].keys()) {
            SeriesEx[s][key]->setVisible(true);
        }
    }else chart->legend()->setVisible(false);

    updateAxisRanges(Series[s]);
    yAxis->setTitleText(nameMap[s][0]);//标题
    xAxis->setTitleText(nameMap[s][1]);//标题
}

void chartForm::getSeries(QString &SeriesTitle, QLineSeries *&ss){
    if(Series.keys().contains(SeriesTitle))
        ss = Series[SeriesTitle];
    else
        ss =new QLineSeries;
}

void chartForm::getCurrentSeries(QString &SeriesTitle,QLineSeries *&ss){
    foreach (QString key, Series.keys()) {
        if(Series[key]->isVisible()){
            SeriesTitle = key;
            ss = Series[SeriesTitle];
            break;
        }
    }
}

void chartForm::getAllSeries(QMap<QString,QLineSeries*>* smap){

    if(!Series.empty())
        *smap = Series;
}

void chartForm::shortShow(int pointsCounts){
    showCounts = pointsCounts;
}

void chartForm::longShow(){
    showCounts = -1;
}

// //实时滤波
// if(MainWindow::systemMap["advance"]["enableFilter_timely"]=="yes"){
//     int window = MainWindow::systemMap["advance"]["filterWindow_timely"].toInt();


//     if(MainWindow::systemMap["advance"]["filterMethod_timely"]=="平均加权"){
//         if (axisPointsList[0].length()>window){
//             for (int i = 0;i<window;i++){
//                 force = force+axisPointsList[0][axisPointsList[0].length()-i-1] + zreoForce;
//             }
//             force = force/(window+1);
//         }
//     }else if(MainWindow::systemMap["advance"]["filterMethod_timely"]=="中值滤波"){
//         if (axisPointsList[0].length()>window){
//             QList<float> temp;
//             for (int i = 0;i<window;i++){
//                 temp.append(axisPointsList[0][axisPointsList[0].length()-i-1] + zreoForce);
//             }
//             temp.append(force);
//             force = Tool::median(temp);
//         }
//     }
// }

void chartForm::addPoint(QString s , QPointF f){

    int cnts = (*Series[s]).count();

    if(smoothType=="滑动平均" && cnts > smoothWindow){

        float sumY = f.y();
        for (int i = 1; i < smoothWindow; i++) {
            sumY += (*Series[s]).at(cnts-i).y();
        }
        float value = sumY/float(smoothWindow);

        QPointF smoothedPoint(f.x(), value);

        *Series[s]<<smoothedPoint;

    }else if(smoothType=="指数加权"&& cnts > smoothWindow){
        float previousEMA = (*Series[s]).at(cnts - 1).y();
        float emaValue = 0.2 * f.y() + (1 - 0.2) * previousEMA;
        QPointF smoothedPoint(f.x(), emaValue);
        *Series[s] << smoothedPoint;
    }
    else
        *Series[s]<<f;




    if(showCounts>0&&Series[s]->count()>showCounts){
        Series[s]->remove(0);
    }

    if(Series[s]->isVisible())
        updateAxisRanges(Series[s]);
    update();
}

void chartForm::addPointEx(QString SeriesTitle, QString subTitle, QPointF Point){
    if(!SeriesEx.keys().contains(SeriesTitle))return;
    if(!SeriesEx[SeriesTitle].keys().contains(subTitle))return;

    *SeriesEx[SeriesTitle][subTitle]<<Point;

    if(showCounts>0&&SeriesEx[SeriesTitle][subTitle]->count()>showCounts){
        SeriesEx[SeriesTitle][subTitle]->remove(0);
    }
    update();
}



void chartForm::addPoints(QString s, QList<QPointF>fl){
    foreach (QPointF f, fl) {
        *Series[s]<<f;
    }
    if(Series[s]->isVisible())
        updateAxisRanges(Series[s]);
    update();
}

void chartForm::addPointsEx(QString SeriesTitle, QString subTitle, QList<QPointF> Points){
    qDebug()<<Points;
    foreach (QPointF f, Points) {
        addPointEx(SeriesTitle,subTitle, f);
        updateAxisRanges(SeriesEx[SeriesTitle][subTitle]);
    }

}

void chartForm::clearSeries(QString s){
    if(!Series.keys().contains(s))return;

    Series[s]->clear();

}

void chartForm::clearSubSeries(QString SeriesTitle, QString subTitle){
    SeriesEx[SeriesTitle][subTitle]->clear();
}

void chartForm::clearAll(){
    foreach (QString s, Series.keys()) {
        clearSeries(s);
        foreach (QString sub, SeriesEx[s].keys()) {
            clearSubSeries(s,sub);
        }
    }
}

void chartForm::setMeaaage(QString m ){
    ui->counts->setText(m);
}

void chartForm::showBar(bool flag){
    if(flag)
        ui->widget_bar->show();
    else
        ui->widget_bar->hide();
}

void saveCsv(QString path,QMap<QString, QLineSeries*> ss){

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("Cannot open file for writing: %s", qPrintable(file.errorString()));
        return;
    }

    QTextStream out(&file);

    int maxCounts = 0;

    foreach (QString key, ss.keys()){
        out << key+"-X,"<<key+"-Y,";
        maxCounts=maxCounts>ss[key]->count()?maxCounts:ss[key]->count();
    }
    out <<"\n";

    for (int i =0;i<maxCounts;i++){
        foreach (QString key, ss.keys()){
            QLineSeries *s = ss[key];
            if(i<s->count()){
                out <<s->points()[i].x()<< ','<<s->points()[i].y();
            }
        }
        out <<"\n";
    }
    file.close();
}


void chartForm::saveAllPoints(const QString defualtName){
    QString saveFilePath = QFileDialog::getSaveFileName(
        this,
        tr("Save File"),
        defualtName,
        defualtName+"(*.csv)");

    QFuture<void> future = QtConcurrent::run(saveCsv,saveFilePath,Series);
    future.waitForFinished();
}

