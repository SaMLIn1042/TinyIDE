#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.resize(1000,700);//设置对话框的长1000宽700

    w.show();

    return a.exec();
}
