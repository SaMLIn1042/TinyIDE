#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMainWindow>
#include <QAction>
#include <QPlainTextEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setCentralWidget(ui->centralwidget);
    setWindowTitle("TinyIDE");
    resize(1920, 1080);
    statusBar()->showMessage("就绪");
}

MainWindow::~MainWindow()
{
    delete ui;
}

