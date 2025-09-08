#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "editor.h"
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
    //setCentralWidget(m_editor);
    connect(m_compiler, &Compiler::compileFinished, this, &MainWindow::onCompileFinished);
    connect(m_compiler, &Compiler::runFinished, this, &MainWindow::onRunFinished);
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
    QString code = ui->editor->getCodeText();
    // 交给编译器去编译
    m_compiler->compile(code);
    // 可以在状态栏显示“编译中...”
    statusBar()->showMessage("编译中...");
}

void MainWindow::on_actionRun_triggered() {
    ui->outputTextEdit->appendPlainText("\n正在运行程序...");
    m_compiler->runProgram();
}

// 8. 实现处理编译结果的槽函数
void MainWindow::onCompileFinished(bool success, const QString &output) {
    if (success) {
        statusBar()->showMessage("编译成功！");
        ui->outputTextEdit->appendPlainText("=== 编译成功 ===");
        ui->outputTextEdit->appendPlainText(output);
    } else {
        statusBar()->showMessage("编译失败！");
        // 显示错误信息output
        ui->outputTextEdit->appendPlainText(output);
    }
}

void MainWindow::onRunFinished(bool success, const QString &output) {
    ui->outputTextEdit->appendPlainText("\n=== 运行结果 ===");
    ui->outputTextEdit->appendPlainText(output);

    if (success) {
        statusBar()->showMessage("程序运行完成！");
    } else {
        statusBar()->showMessage("程序运行失败！");
    }

    // 确保输出面板显示最新内容
    ui->outputTextEdit->moveCursor(QTextCursor::End);
}
