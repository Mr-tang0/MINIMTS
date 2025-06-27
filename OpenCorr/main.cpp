#include "opencorr_w.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    OpenCorr_W w;
    w.show();
    return a.exec();
}
