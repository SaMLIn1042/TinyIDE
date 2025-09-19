#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QMenuBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    /*设置标题*/
    setWindowTitle("samlinpad");

    connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::on_actionQuit_triggered);
}

void MainWindow::on_actionQuit_triggered()
{
    this->close();
}

MainWindow::~MainWindow()
{
    delete ui;
}

