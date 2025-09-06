#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QWidget>
#include <QVBoxLayout>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("C IDE");
        resize(600, 400);

        // 菜单栏
        QMenuBar* bar = menuBar();

        QMenu* fileMenu = bar->addMenu("File");
        QAction *newFileAction = new QAction("New File or Project", this);
        QAction *openFileAction = new QAction("Open File or Project", this);
        fileMenu->addAction(newFileAction);
        fileMenu->addAction(openFileAction);

        connect(newFileAction, &QAction::triggered, this, &MainWindow::onNewFileOrProject);
        connect(openFileAction, &QAction::triggered, this, &MainWindow::onOpenFileOrProject);

        bar->addMenu("Edit");
        bar->addMenu("Build");
        bar->addMenu("Run");
        bar->addMenu("Tools");
        bar->addMenu("Help");

        // 右上角按钮
        closeButton = new QPushButton("Close all", this);
        connect(closeButton, &QPushButton::clicked, this, &MainWindow::onCloseClicked);
        bar->setCornerWidget(closeButton, Qt::TopRightCorner);

        // 中心部件可以留空，将来放编辑器
        //QWidget* centralWidget = new QWidget(this);
        //setCentralWidget(centralWidget);

        //左侧工具栏
        leftToolBar = new QToolBar("Files", this);
        leftToolBar->setOrientation(Qt::Vertical);
        leftToolBar->addAction("find");
        leftToolBar->addAction("System Settings");
        addToolBar(Qt::LeftToolBarArea, leftToolBar);

        //左右分割
        fileListWidget = new QListWidget(this);
        fileListWidget->addItem("main.c");
        fileListWidget->addItem("code.c");//这两行暂时随便写一下，模拟之后放文件列表的地方

        codeEditor = new QTextEdit(this);
        codeEditor->setText("// Write your C code here...");

        QSplitter *splitter = new QSplitter(this);
        splitter->addWidget(fileListWidget);
        splitter->addWidget(codeEditor);
        splitter->setStretchFactor(0, 1); // 左边占较小比例
        splitter->setStretchFactor(1, 3); // 右边占较大比例

        setCentralWidget(splitter);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onNewFileOrProject()
{
    QMessageBox::information(this, "New", "Create a new file or project!");
}

void MainWindow::onOpenFileOrProject()
{
    QMessageBox::information(this, "Open", "Open a new file or project!");
}

void MainWindow::onCloseClicked()
{
    close();  // 关闭窗口
}
