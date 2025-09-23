#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    text1 = new QTextEdit;
    QFont f;
    f.setPixelSize(30);
    text1->setFont(f);
    this->setCentralWidget(text1);

    file = this->menuBar()->addMenu("文件");
    edit = this->menuBar()->addMenu("编辑");
    help = this->menuBar()->addMenu("帮助");

    file_open = new QAction("打开", this); file_open->setShortcut(tr("Ctrl+O"));
    file_exit = new QAction("关闭", this); file->addAction(file_open);
    file->addSeparator(); file->addAction(file_exit);
}

MainWindow::~MainWindow()
{
    delete text1;
}

