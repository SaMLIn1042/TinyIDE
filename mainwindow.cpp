#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScrollBar>
#include <QStatusBar>
#include <QDebug>
#include <QSplitter>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QListWidget>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

// 初始化UI和核心组件
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_currentFilePath(""),
      m_isSaved(false)
{
    ui->setupUi(this);

    // 获取编辑器组件
    m_editor = ui->editor;

    // 设置初始示例代码
    QString initialCode =
        "#include <stdio.h>\n\n"
        "int main() {\n"
        "    scanf(\"%d\");\n"
        "    printf(\"Hello, World!\\n\");\n"
        "    scanf(\"%d\");\n"
        "    return 0;\n"
        "}";
    m_editor->setPlainText(initialCode);
    m_editor->setProperty("originalText", initialCode);
    // 在Editor类中可以添加一个setOriginalText方法来设置m_originalText
    m_editor->setOriginalText(initialCode); // 初始化原始文本用于跟踪新增行

    // 连接编辑器内容变化信号
    connect(m_editor, &QPlainTextEdit::textChanged,
            this, &MainWindow::onEditorTextChanged);

    // 创建左侧文件列表
    fileListWidget = new QListWidget(this);
    fileListWidget->addItem("main.c");
    fileListWidget->addItem("utils.c");
    fileListWidget->addItem("functions.c");
    fileListWidget->setMaximumWidth(150);

    // 设置为只读（禁止编辑）
    ui->outputTextEdit->setReadOnly(true);

    // 禁用文本编辑功能（防止通过上下文菜单修改）
    ui->outputTextEdit->setUndoRedoEnabled(false);

    // 禁用拖放功能（防止拖入内容）
    ui->outputTextEdit->setAcceptDrops(false);

    // 创建输入区域
    QWidget *inputWidget = new QWidget(this);
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 5, 0, 0);

    QLabel *inputLabel = new QLabel("输入:", this);
    m_inputLineEdit = new QLineEdit(this);
    QPushButton *sendButton = new QPushButton("发送", this);

    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(m_inputLineEdit);
    inputLayout->addWidget(sendButton);

    // 连接发送按钮
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendInput);
    connect(m_inputLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendInput);

    // 初始时禁用输入区域
    inputWidget->setEnabled(false);

    // 创建右侧垂直布局（编辑器 + 输出框 + 输入区域）
    QWidget *rightWidget = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(ui->editor);
    rightLayout->addWidget(ui->outputTextEdit);
    rightLayout->addWidget(inputWidget);

    // 创建主水平分割器（左侧文件列表 + 右侧布局）
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(fileListWidget);
    mainSplitter->addWidget(rightWidget);

    // 设置主分割比例
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);

    // 创建带边距的容器
    QWidget *container = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(10, 10, 10, 10);
    containerLayout->addWidget(mainSplitter);

    // 设置中央部件
    setCentralWidget(container);

    // 初始化编译器对象
    m_compiler = new Compiler(this);

    // 连接编译器信号
    connect(m_compiler, &Compiler::compileFinished,
            this, &MainWindow::onCompileFinished);

    connect(m_compiler, &Compiler::runFinished,
            this, &MainWindow::onRunFinished);

    connect(m_compiler, &Compiler::runOutput,
            this, &MainWindow::handleRunOutput);

    // 设置初始窗口标题
    setWindowTitle("TinyIDE - 未命名");

    // 连接文件列表点击事件
    connect(fileListWidget, &QListWidget::itemClicked,
            this, &MainWindow::onFileItemClicked);

    // 设置停止按钮的快捷键和初始状态
    ui->actionStop->setShortcut(QKeySequence(Qt::Key_Escape));
    ui->actionStop->setEnabled(false);

    // 保存输入区域组件指针
    m_inputWidget = inputWidget;

    // 连接运行状态变化信号，动态启用/禁用输入区域和停止按钮
    connect(m_compiler, &Compiler::runStarted, this, [this]()
            {
                ui->actionStop->setEnabled(true);
                m_inputWidget->setEnabled(true); // 启用输入区域
                m_inputLineEdit->setFocus();     // 焦点设置到输入框
            });

    connect(m_compiler, &Compiler::runFinished, this, [this](bool success, const QString &output)
            {
                Q_UNUSED(success)
                Q_UNUSED(output)
                ui->actionStop->setEnabled(false);
                m_inputWidget->setEnabled(false); // 禁用输入区域
            });
}

// 析构函数：清理资源
MainWindow::~MainWindow()
{
    delete ui;
}

// 更新文件列表（示例实现）
void MainWindow::updateFileList()
{
    fileListWidget->clear();
    fileMap.clear();

    // 添加示例文件列表
    QStringList files = {
        "main.c",
        "utils.c",
        "functions.c",
        "headers.h"};

    // 创建模拟文件路径并添加到列表
    foreach (const QString &file, files)
    {
        QString filePath = QDir::homePath() + "/TinyIDE/" + file;
        fileMap[file] = filePath; // 存储文件名到路径的映射

        QListWidgetItem *item = new QListWidgetItem(file);
        fileListWidget->addItem(item);
    }
}

// 文件列表项点击处理
void MainWindow::onFileItemClicked(QListWidgetItem *item)
{
    QString fileName = item->text();
    QString filePath = fileMap.value(fileName);

    // 检查文件路径有效性
    if (filePath.isEmpty())
    {
        QMessageBox::warning(this, "文件错误", "无法找到文件路径");
        return;
    }

    // 检查未保存的更改
    if (!m_isSaved)
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存提示",
                                      "当前文件有未保存的更改，是否保存？",
                                      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel)
        {
            return; // 取消操作
        }
        else if (reply == QMessageBox::Save)
        {
            if (!on_actionSave_triggered())
            {
                return; // 保存失败取消操作
            }
        }
    }

    // 打开并读取文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "错误", "无法打开文件: " + file.errorString());
        return;
    }

    // 显示文件内容
    QTextStream in(&file);
    m_editor->setPlainText(in.readAll());
    file.close();

    // 更新文件状态
    m_currentFilePath = filePath;
    m_isSaved = true;
    setWindowTitle("TinyIDE - " + fileName);
}

// 编译操作处理
void MainWindow::on_actionCompile_triggered()
{
    // 添加编译分隔线
    ui->outputTextEdit->appendPlainText("\n--- 开始编译 ---");
    statusBar()->showMessage("编译中...");

    // 获取并编译当前代码
    QString code = m_editor->getCodeText();
    m_compiler->compile(code);
}

// 运行操作处理
void MainWindow::on_actionRun_triggered()
{
    ui->outputTextEdit->appendPlainText("\n--- 运行程序 ---");
    statusBar()->showMessage("运行中...");
    m_compiler->runProgram();
}

// 编译完成处理
void MainWindow::onCompileFinished(bool success, const QString &output)
{
    // 更新状态栏和输出框
    statusBar()->showMessage(success ? "编译成功" : "编译失败");
    ui->outputTextEdit->appendPlainText(output);

    // 自动滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

// 运行完成处理
void MainWindow::onRunFinished(bool success, const QString &output)
{
    statusBar()->showMessage(success ? "运行完成" : "运行失败");
    ui->outputTextEdit->appendPlainText(output);

    // 自动滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

// 运行时输出处理
void MainWindow::handleRunOutput(const QString &output)
{
    ui->outputTextEdit->appendPlainText(output);

    // 自动滚动到底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

// 新建文件处理
void MainWindow::on_actionNew_triggered()
{
    // 检查未保存的更改
    if (!m_isSaved)
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存提示",
                                      "当前文件有未保存的更改，是否保存？",
                                      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel)
        {
            return;
        }
        else if (reply == QMessageBox::Save)
        {
            if (!on_actionSave_triggered())
            {
                return;
            }
        }
    }

    // 重置编辑器状态
    m_editor->clear();
    m_currentFilePath = "";
    m_isSaved = true;
    setWindowTitle("TinyIDE - 未命名");
    ui->outputTextEdit->clear(); // 清空输出框
}

// 打开文件处理
void MainWindow::on_actionOpen_triggered()
{
    // 检查未保存的更改
    if (!m_isSaved)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "保存提示", "当前文件有未保存的更改，是否保存？",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel)
        {
            return;
        }
        else if (reply == QMessageBox::Save)
        {
            if (!on_actionSave_triggered())
            {
                return;
            }
        }
    }

    // 显示文件选择对话框
    QString filePath = QFileDialog::getOpenFileName(
        this, "打开文件", QDir::homePath(), "C源文件 (*.c);;所有文件 (*)");
    if (filePath.isEmpty())
    {
        return; // 用户取消操作
    }

    // 读取文件内容
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "错误", "无法打开文件: " + file.errorString());
        return;
    }

    // 显示文件内容
    QTextStream in(&file);
    m_editor->setPlainText(in.readAll());
    file.close();

    // 更新文件状态
    m_currentFilePath = filePath;
    m_isSaved = true;
    setWindowTitle("TinyIDE - " + QFileInfo(filePath).fileName());
}

// 保存文件处理
bool MainWindow::on_actionSave_triggered()
{
    // 无路径时调用另存为
    if (m_currentFilePath.isEmpty())
    {
        return on_actionSaveAs_triggered();
    }

    // 写入文件
    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "错误", "无法保存文件: " + file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << m_editor->getCodeText();
    file.close();

    // 更新保存状态
    m_isSaved = true;
    statusBar()->showMessage("文件已保存: " + m_currentFilePath);
    // 添加：更新窗口标题为当前文件名
    setWindowTitle("TinyIDE - " + QFileInfo(m_currentFilePath).fileName());

    return true;
}

// 另存为文件处理
bool MainWindow::on_actionSaveAs_triggered()
{
    // 显示保存对话框
    QString filePath = QFileDialog::getSaveFileName(
        this, "另存为", QDir::homePath(), "C源文件 (*.c);;所有文件 (*)");
    if (filePath.isEmpty())
    {
        return false; // 用户取消操作
    }

    // 确保.c扩展名
    if (!filePath.endsWith(".c", Qt::CaseInsensitive))
    {
        filePath += ".c";
    }

    // 更新路径并保存
    m_currentFilePath = filePath;
    bool result = on_actionSave_triggered();

    return result;
}

// 关闭文件处理
void MainWindow::on_actionClose_triggered()
{
    // 检查未保存的更改
    if (!m_isSaved)
    {
        QString fileName = m_currentFilePath.isEmpty() ? "未命名文件" : QFileInfo(m_currentFilePath).fileName();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存提示",
                                      QString("%1 已修改，是否保存？").arg(fileName),
                                      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel)
        {
            return;
        }
        else if (reply == QMessageBox::Save)
        {
            if (!on_actionSave_triggered())
            {
                return;
            }
        }
    }

    // 重置编辑器状态
    m_editor->clear();
    m_currentFilePath.clear();
    m_isSaved = false;
    setWindowTitle("TinyIDE");
    statusBar()->showMessage("文件已关闭");
}

// 退出应用处理
void MainWindow::on_actionExit_triggered()
{
    // 检查未保存的更改
    if (!m_isSaved)
    {
        QString fileName = m_currentFilePath.isEmpty() ? "未命名文件" : QFileInfo(m_currentFilePath).fileName();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "保存提示",
                                      QString("%1 已修改，是否保存？").arg(fileName),
                                      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel)
        {
            return;
        }
        else if (reply == QMessageBox::Save)
        {
            if (!on_actionSave_triggered())
            {
                return;
            }
        }
    }

    // 退出应用
    qApp->quit();
}

void MainWindow::on_actionStop_triggered()
{
    // 调用编译器的停止方法
    m_compiler->stopProgram();
    statusBar()->showMessage("程序已停止");
}

// 编辑器内容变化处理
void MainWindow::onEditorTextChanged()
{
    // 标记文件为未保存状态
    if (m_isSaved)
    {
        m_isSaved = false;

        // 在窗口标题添加*标记
        QString title = "TinyIDE - ";
        if (m_currentFilePath.isEmpty())
        {
            title += "未命名";
        }
        else
        {
            title += QFileInfo(m_currentFilePath).fileName();
        }
        title += "*"; // 未保存标记
        setWindowTitle(title);
    }
}

void MainWindow::onSendInput()
{
    QString input = m_inputLineEdit->text();
    if (!input.isEmpty())
    {
        // 发送输入到运行中的程序
        m_compiler->sendInput(input);

        // 在输出框中显示输入内容
        ui->outputTextEdit->appendPlainText("> " + input);

        // 清空输入框
        m_inputLineEdit->clear();
    }
}
