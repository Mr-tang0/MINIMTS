#include "hikcamera.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    HIKCamera w;
    w.show();
    w.enableControlWidget(true);
    w.enableStateWidget(true);
    return a.exec();
}
