#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScrollBar>
#include <QStatusBar>
#include <QDebug>
#include <QSplitter>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 确保在UI文件中正确命名了组件
    m_editor = ui->editor; // 直接使用UI中的组件

    // 设置初始测试程序
    QString initialCode =
        "#include <stdio.h>\n\n"
        "int main() {\n"
        "    printf(\"Hello, World!\\n\");\n"
        "    return 0;\n"
        "}";
    m_editor->setPlainText(initialCode);

    // 创建垂直分割器并设置比例
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(ui->editor);
    splitter->addWidget(ui->outputTextEdit);

    // 设置比例（7:3）
    splitter->setStretchFactor(0, 7);
    splitter->setStretchFactor(1, 3);

    // 创建一个容器widget来添加边距
    QWidget *container = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(10, 0, 10, 0); // 左右各10像素边距
    containerLayout->addWidget(splitter);

    // 设置中央部件
    setCentralWidget(container);

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

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionCompile_triggered()
{
    // 清空输出区域
    ui->outputTextEdit->clear();
    ui->outputTextEdit->appendPlainText("开始编译...");
    statusBar()->showMessage("编译中...");

    // 使用UI中的editor获取代码
    QString code = m_editor->getCodeText();
    m_compiler->compile(code);
}

void MainWindow::on_actionRun_triggered()
{
    ui->outputTextEdit->appendPlainText("\n--- 运行程序 ---");
    statusBar()->showMessage("运行中...");
    m_compiler->runProgram();
}

void MainWindow::onCompileFinished(bool success, const QString &output)
{
    statusBar()->showMessage(success ? "编译成功" : "编译失败");
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void MainWindow::onRunFinished(bool success, const QString &output)
{
    statusBar()->showMessage(success ? "运行完成" : "运行失败");
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void MainWindow::handleRunOutput(const QString &output)
{
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}
