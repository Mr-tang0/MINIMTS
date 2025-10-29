#ifndef QTOPENCORR_H
#define QTOPENCORR_H

#include "QTOpenCorr_global.h"
#include <fstream>
#include "opencorr.h"
#include <QObject>


using namespace opencorr;
using namespace std;

class QTOPENCORR_EXPORT QTOpenCorr : public QObject
{
    Q_OBJECT
public:
    explicit QTOpenCorr(QObject *parent = nullptr);
    ~QTOpenCorr();

    int getCPUThread();

    void setCPUThread(int thread);

    void setDIC(int subset_x = 16, int subset_y = 16, int poi_x = 10, int poi_y = 10, int space = 2){
        subset_radius_x = subset_x;
        subset_radius_y = subset_y;
        poi_number_x = poi_x;
        poi_number_y = poi_y;
        grid_space = 2;
    };

    void setLeftTop(int l = 10, int t =10){
        left = l;
        top = t;
    }

public slots:
    QPointF calculate(cv::Mat ori, cv::Mat tar);
    QPointF calculate(string ori, string tar);

private:
    int cpu_thread_number = 1;
    int target_thread = 1;

    //set DIC parameters
    int subset_radius_x = 16;
    int subset_radius_y = 16;
    int max_iteration = 10;
    float max_deformation_norm = 0.001f;

    //set POIs
    vector<POI2D> poi_queue;
    int poi_number_x = 10;
    int poi_number_y = 10;
    int grid_space = 2;

    int left = 70;
    int top = 70;

    FFTCC2D* fftcc;
    ICGN2D1* icgn1;

    float testmean = 0;

};


#endif // QTOPENCORR_H
