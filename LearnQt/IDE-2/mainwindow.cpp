#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    text1 = new QTextEdit;
    QFont f;
    f.setPixelSize(30);//设置字体大小为30
    f.setWeight(QFont::Bold);//设置字体粗细
    text1->setFont(f);
    QColor c;
    c.setRgb(0,0,0);
    text1->setTextColor(c);//设置字体颜色为黑色
    this->setCentralWidget(text1);

    file = this->menuBar()->addMenu("文件");//添加一个菜单项
    edit = this->menuBar()->addMenu("编辑");//添加一个菜单项
    help = this->menuBar()->addMenu("帮助");//添加一个菜单项

    file_create = new QAction("新建文件或项目",this);
    file_create->setShortcut(tr("Ctrl+N"));
    file_open = new QAction("打开文件或项目",this);
    file_open->setShortcut(tr("Ctrl+O"));
    file->addAction(file_create);
    file->addAction(file_open);
}

MainWindow::~MainWindow()
{
    delete text1;
}

