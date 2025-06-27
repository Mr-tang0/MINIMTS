#ifndef OPENCORR_W_H
#define OPENCORR_W_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class OpenCorr_W;
}
QT_END_NAMESPACE

class OpenCorr_W : public QWidget
{
    Q_OBJECT

public:
    OpenCorr_W(QWidget *parent = nullptr);
    ~OpenCorr_W();

private:
    Ui::OpenCorr_W *ui;
};
#endif // OPENCORR_W_H
