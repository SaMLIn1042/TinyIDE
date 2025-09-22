#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("IDE Test");
}

void MainWindow::on_actionQuit_triggered()
{
    this->close();
}

MainWindow::~MainWindow()
{
    delete ui;
}

