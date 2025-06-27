#include "ncorr_w.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Ncorr_W w;
    w.show();
    return a.exec();
}
