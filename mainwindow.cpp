#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScrollBar>
#include <QStatusBar>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 使用UI中已有的编辑器组件
    m_editor = findChild<Editor*>("editor");
    if(!m_editor) {
        qDebug() << "警告：未找到editor组件，使用默认位置";
        m_editor = new Editor(this);
        // 添加到中央区域（兼容旧UI设计）
        setCentralWidget(m_editor);
    }

    // 初始化编译器
    m_compiler = new Compiler(this);

    // 连接信号
    connect(m_compiler, &Compiler::compileFinished,
            this, &MainWindow::onCompileFinished);

    connect(m_compiler, &Compiler::runFinished,
            this, &MainWindow::onRunFinished);

    connect(m_compiler, &Compiler::runOutput,
            this, &MainWindow::handleRunOutput);
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_actionCompile_triggered() {
    // 清空输出区域
    ui->outputTextEdit->clear();
    ui->outputTextEdit->appendPlainText("开始编译...");
    statusBar()->showMessage("编译中...");

    // 使用UI中的editor获取代码
    QString code = m_editor->getCodeText();
    m_compiler->compile(code);
}

void MainWindow::on_actionRun_triggered() {
    ui->outputTextEdit->appendPlainText("\n--- 运行程序 ---");
    statusBar()->showMessage("运行中...");
    m_compiler->runProgram();
}

void MainWindow::onCompileFinished(bool success, const QString &output) {
    statusBar()->showMessage(success ? "编译成功" : "编译失败");
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void MainWindow::onRunFinished(bool success, const QString &output) {
    statusBar()->showMessage(success ? "运行完成" : "运行失败");
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void MainWindow::handleRunOutput(const QString &output) {
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}
