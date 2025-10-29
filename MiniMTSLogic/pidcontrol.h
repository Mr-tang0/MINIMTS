#ifndef PIDCONTROL_H
#define PIDCONTROL_H

#include <QWidget>

namespace Ui {
class PIDControl;
}

class PIDControl : public QWidget
{
    Q_OBJECT

public:
    explicit PIDControl(QWidget *parent = nullptr);
    ~PIDControl();

    void initPID(QMap<QString, QString>);

private:
    Ui::PIDControl *ui;

signals:
    void savePID(float p, float i, float d, float dt, float dir, float maxo, float mino);
private slots:
    void on_pushButton_clicked();
};

#endif // PIDCONTROL_H
