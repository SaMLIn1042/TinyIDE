#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    /*设置标题*/
    setWindowTitle("samlinpad");
}

MainWindow::~MainWindow()
{
    delete ui;
}

