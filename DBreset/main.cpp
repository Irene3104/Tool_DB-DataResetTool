#include "DBreset.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    DBreset w;
    w.show();
    return a.exec();
}
