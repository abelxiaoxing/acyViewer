#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName("MyCompany");
    QApplication::setApplicationName("acyViewer");

    MainWindow w;
    w.show();

    return a.exec();
}
