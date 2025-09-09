#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScrollBar>
#include <QStatusBar>
#include <QDebug>
#include <QSplitter>
#include <QVBoxLayout>
#include <QMessageBox>    // 用于消息提示框
#include <QFileInfo>      // 用于文件操作
#include <QFileDialog>    // 文件对话框
#include <QFile>          // 文件操作
#include <QTextStream>    // 文本流



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
       // 新增：初始化文件状态
      m_currentFilePath(""),
      m_isSaved(false)
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

    // 新增：连接编辑器内容变化信号
    connect(m_editor, &QPlainTextEdit::textChanged,
            this, &MainWindow::onEditorTextChanged);

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
    // 新增：初始化窗口标题
    setWindowTitle("TinyIDE - 未命名");
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
// 新增：新建文件功能
void MainWindow::on_actionNew_triggered()
{
    // 检查当前文件是否有未保存的更改
    if (!m_isSaved) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存提示",
                                     "当前文件有未保存的更改，是否保存？",
                                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel) {
            return; // 取消新建操作
        } else if (reply == QMessageBox::Save) {
            // 如果选择保存，调用保存函数
            if (!on_actionSave_triggered()) {
                return; // 保存失败则取消新建
            }
        }
        // 选择Discard则直接清空
    }

    // 清空编辑器并重置状态
    m_editor->clear();
    m_currentFilePath = "";
    m_isSaved = true;
    setWindowTitle("TinyIDE - 未命名");
    ui->outputTextEdit->clear();
}

// 新增：打开文件功能
void MainWindow::on_actionOpen_triggered()
{
    // 检查当前文件是否未保存
    if (!m_isSaved) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "保存提示", "当前文件有未保存的更改，是否保存？",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
        );
        if (reply == QMessageBox::Cancel) {
            return; // 取消打开操作
        } else if (reply == QMessageBox::Save) {
            if (!on_actionSave_triggered()) {
                return; // 保存失败则取消打开
            }
        }
    }

    // 打开文件对话框，选择C语言文件
    QString filePath = QFileDialog::getOpenFileName(
        this, "打开文件", QDir::homePath(), "C源文件 (*.c);;所有文件 (*)"
    );
    if (filePath.isEmpty()) {
        return; // 用户取消选择
    }

    // 读取文件内容
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法打开文件: " + file.errorString());
        return;
    }

    // 显示文件内容到编辑器
    QTextStream in(&file);
    QString content = in.readAll();
    m_editor->setPlainText(content);
    file.close();

    // 更新文件状态
    m_currentFilePath = filePath;
    m_isSaved = true;
    setWindowTitle("TinyIDE - " + QFileInfo(filePath).fileName());
}

// 保存文件
bool MainWindow::on_actionSave_triggered()
{
    // 如果没有当前文件路径，调用另存为
    if (m_currentFilePath.isEmpty()) {
        return on_actionSaveAs_triggered();
    }

    // 写入文件
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法保存文件: " + file.errorString());
        return false; // 保存失败
    }

    QTextStream out(&file);
    out << m_editor->getCodeText();
    file.close();

    m_isSaved = true; // 标记为已保存
    statusBar()->showMessage("文件已保存: " + m_currentFilePath);
    return true; // 保存成功
}

// 另存为文件
bool MainWindow::on_actionSaveAs_triggered()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "另存为", QDir::homePath(), "C源文件 (*.c);;所有文件 (*)"
    );
    if (filePath.isEmpty()) {
        return false; // 用户取消操作
    }

    // 确保文件扩展名为 .c
    if (!filePath.endsWith(".c", Qt::CaseInsensitive)) {
        filePath += ".c";
    }

    m_currentFilePath = filePath;
    return on_actionSave_triggered(); // 调用保存函数
}
// 新增：关闭当前文件功能
void MainWindow::on_actionClose_triggered()
{
    // 如果文件未保存，提示保存
    if (!m_isSaved) {
        QString fileName = m_currentFilePath.isEmpty() ? "未命名文件" : QFileInfo(m_currentFilePath).fileName();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存提示",
                                     QString("%1 已修改，是否保存？").arg(fileName),
                                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel) {
            return; // 取消关闭操作
        } else if (reply == QMessageBox::Save) {
            if (!on_actionSave_triggered()) {
                return; // 保存失败，取消关闭
            }
        }
        // 选择Discard则直接关闭，不保存
    }

    // 清空编辑器内容和文件路径
    m_editor->clear();
    m_currentFilePath.clear();
    m_isSaved = false;
    setWindowTitle("TinyIDE");
    statusBar()->showMessage("文件已关闭");
}

// 新增：退出应用程序功能
void MainWindow::on_actionExit_triggered()
{
    // 如果有未保存的内容，先提示保存
    if (!m_isSaved) {
        QString fileName = m_currentFilePath.isEmpty() ? "未命名文件" : QFileInfo(m_currentFilePath).fileName();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存提示",
                                     QString("%1 已修改，是否保存？").arg(fileName),
                                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel) {
            return; // 取消退出
        } else if (reply == QMessageBox::Save) {
            if (!on_actionSave_triggered()) {
                return; // 保存失败，取消退出
            }
        }
        // 选择Discard则直接退出，不保存
    }

    // 退出应用程序
    qApp->quit();
}

// 新增：跟踪编辑器内容变化
void MainWindow::onEditorTextChanged()
{
    if (m_isSaved) { // 只有当前是已保存状态时才更新（避免重复触发）
        m_isSaved = false;
        // 窗口标题添加*表示未保存
        QString title = "TinyIDE - ";
        if (m_currentFilePath.isEmpty()) {
            title += "未命名";
        } else {
            title += QFileInfo(m_currentFilePath).fileName();
        }
        title += "*"; // *表示未保存
        setWindowTitle(title);
    }
}
