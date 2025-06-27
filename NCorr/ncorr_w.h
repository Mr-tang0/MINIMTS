#ifndef NCORR_W_H
#define NCORR_W_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class Ncorr_W;
}
QT_END_NAMESPACE

class Ncorr_W : public QWidget
{
    Q_OBJECT

public:
    Ncorr_W(QWidget *parent = nullptr);
    ~Ncorr_W();

private:
    Ui::Ncorr_W *ui;
};
#endif // NCORR_W_H
