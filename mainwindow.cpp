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
    m_editor = new Editor(this);
    m_compiler = new Compiler(this);
    setCentralWidget(m_editor);
    connect(m_compiler, &Compiler::compileFinished, this, &MainWindow::onCompileFinished);
    setWindowTitle("TinyIDE");
    resize(1920, 1080);
    statusBar()->showMessage("就绪");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionCompile_triggered() {
    // 从编辑器获取代码
    QString code = m_editor->getCodeText();
    // 交给编译器去编译
    m_compiler->compile(code);
    // 可以在状态栏显示“编译中...”
    statusBar()->showMessage("编译中...");
}

// 8. 实现处理编译结果的槽函数
void MainWindow::onCompileFinished(bool success, const QString &output) {
    if (success) {
        statusBar()->showMessage("编译成功！");
        // 可以把output显示在某个你自己添加的文本浏览器部件中
    } else {
        statusBar()->showMessage("编译失败！");
        // 显示错误信息output
    }
}
