#include "editor.h"
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
#include <QTimer>

// 主窗口构造函数，初始化UI和核心组件
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      m_currentFilePath(""),
      m_isSaved(false)
{
    ui->setupUi(this);

    // 高亮功能相关操作
    QAction *aHighlight = new QAction(tr("高亮所选"), this);
    aHighlight->setObjectName("actionHighlightSelection");
    ui->toolBar->addAction(aHighlight);

    QAction *aClear = new QAction(tr("清除高亮"), this);
    aClear->setObjectName("actionClearHighlights");
    ui->toolBar->addAction(aClear);

    ui->actionFind->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    ui->actionReplace->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));

    // 获取编辑器组件引用
    //m_editor = ui->editor;

    // 设置高亮功能
//    m_editor->setHighlightActions(aHighlight, aClear);
//    connect(aHighlight, SIGNAL(triggered()), m_editor, SLOT(highlightSelection()));
//    connect(aClear, SIGNAL(triggered()), m_editor, SLOT(clearAllHighlights()));
      connect(aHighlight, &QAction::triggered, this, [this]() {
            Editor *e = currentEditor();
            if (e) e->highlightSelection();
        });
      connect(aClear, &QAction::triggered, this, [this]() {
            Editor *e = currentEditor();
            if (e) e->clearAllHighlights();
        });

    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true); // 允许关闭标签
    m_tabWidget->setMovable(true);      // 允许拖动标签

    // 获取默认字体
    QFont defaultFont = getDefaultEditorFont();

    // 设置初始示例代码
    QString initialCode =
        "#include <stdio.h>\n\n"
        "int main() {\n"
        "    scanf(\"%d\");\n"
        "    printf(\"Hello, World!\\n\");\n"
        "    scanf(\"%d\");\n"
        "    return 0;\n"
        "}";
    // 创建第一个标签页
    Editor *editor = new Editor();
    editor->setPlainText(initialCode);
    editor->setOriginalText(initialCode);

    // 确保应用默认字体
    editor->setEditorFont(defaultFont);

    // 连接编辑器内容变化信号
    connect(editor, &QPlainTextEdit::textChanged,
            this, &MainWindow::onEditorTextChanged);

    // 添加到标签页
    m_tabWidget->addTab(editor, "未命名");

    // 存储标签页信息
    FileTabInfo info;
    info.editor = editor;
    info.filePath = "";
    info.isSaved = true;
    info.displayName = "未命名";
    m_tabInfos.append(info);

    // 设置当前标签页索引
    m_currentTabIndex = 0;

    // 设置输出框为只读模式
    ui->outputTextEdit->setReadOnly(true);

    // 禁用输出框的撤销/重做功能
    ui->outputTextEdit->setUndoRedoEnabled(false);

    // 禁用输出框的拖放功能
    ui->outputTextEdit->setAcceptDrops(false);

    // 创建程序输入区域
    QWidget *inputWidget = new QWidget(this);
    QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 5, 0, 0);

    QLabel *inputLabel = new QLabel("输入:", this);
    m_inputLineEdit = new QLineEdit(this);
    QPushButton *sendButton = new QPushButton("发送", this);

    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(m_inputLineEdit);
    inputLayout->addWidget(sendButton);

    // 连接输入发送按钮和回车键事件
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendInput);
    connect(m_inputLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendInput);

    // 初始状态下禁用输入区域
    inputWidget->setEnabled(false);

    // 创建主垂直布局（标签页 + 输出框 + 输入区域）
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->addWidget(m_tabWidget);
    mainLayout->addWidget(ui->outputTextEdit);
    mainLayout->addWidget(inputWidget);

    // 设置中央部件
    setCentralWidget(centralWidget);

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
    // 全局查找/替换由 MainWindow 转发到当前编辑器
    connect(ui->actionFind, &QAction::triggered, this, [this]() {
        Editor *e = currentEditor();
        if (e) e->handleFind();
    });
    connect(ui->actionReplace, &QAction::triggered, this, [this]() {
        Editor *e = currentEditor();
        if (e) e->handleReplace();
    });
    connect(aHighlight, &QAction::triggered, this, [this]() {
        Editor *e = currentEditor();
        if (e) e->highlightSelection();
    });
    connect(aClear, &QAction::triggered, this, [this]() {
        Editor *e = currentEditor();
        if (e) e->clearAllHighlights();
    });

    // 设置停止按钮的快捷键和初始状态
    ui->actionStop->setShortcut(QKeySequence(Qt::Key_Escape));
    ui->actionStop->setEnabled(false);

    // 连接标签页信号
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

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

    // 延迟调用，确保编辑器能够找到主窗口
    //QTimer::singleShot(100, this, [this, editor]()
    //                   { editor->findActionsFromMainWindow(); });
}

// 析构函数：清理资源
MainWindow::~MainWindow()
{
    delete ui;
}

// 获取默认编辑器字体
QFont MainWindow::getDefaultEditorFont() const
{
    QFont defaultFont;

    // 更可靠的字体检测方法
    QStringList preferredFonts = {
        "Consolas",
        "Source Code Pro",
        "Monaco",
        "Courier New",
        "Monospace" // 通用等宽字体
    };

    // 检查系统是否有首选字体
    QFontDatabase fontDb;
    QString selectedFont = "Monospace"; // 默认回退

    for (const QString &fontName : preferredFonts)
    {
        if (fontDb.families().contains(fontName))
        {
            selectedFont = fontName;
            qDebug() << "使用字体:" << fontName;
            break;
        }
    }

    defaultFont.setFamily(selectedFont);
    defaultFont.setPointSize(10);
    defaultFont.setStyleHint(QFont::TypeWriter); // 明确指定为等宽字体

    return defaultFont;
}

// 标签页切换处理
void MainWindow::onTabChanged(int index)
{
    if (index < 0 || index >= m_tabInfos.size())
        return;

    m_currentTabIndex = index;
    FileTabInfo &info = m_tabInfos[index];

    // 更新窗口标题
    setWindowTitle("TinyIDE - " + info.displayName + (info.isSaved ? "" : "*"));
}

// 标签页关闭请求处理
void MainWindow::onTabCloseRequested(int index)
{
    if (index < 0 || index >= m_tabInfos.size())
        return;

    FileTabInfo &info = m_tabInfos[index];

    // 检查未保存的更改
    if (!info.isSaved)
    {
        QString fileName = info.filePath.isEmpty() ? "未命名文件" : QFileInfo(info.filePath).fileName();

        // 创建自定义消息框，使用中文按钮
        QMessageBox msgBox(QMessageBox::Question,
                           "保存提示",
                           QString("%1 已修改，是否保存？").arg(fileName),
                           QMessageBox::NoButton, // 不使用默认按钮
                           this);

        // 手动创建中文按钮
        QPushButton *saveBtn = new QPushButton("保存", &msgBox);
        QPushButton *discardBtn = new QPushButton("不保存", &msgBox);
        QPushButton *cancelBtn = new QPushButton("取消", &msgBox);

        // 添加按钮到消息框
        msgBox.addButton(saveBtn, QMessageBox::ActionRole);
        msgBox.addButton(discardBtn, QMessageBox::ActionRole);
        msgBox.addButton(cancelBtn, QMessageBox::ActionRole);

        // 设置默认按钮
        msgBox.setDefaultButton(saveBtn);

        // 显示弹窗
        msgBox.exec();

        if (msgBox.clickedButton() == cancelBtn)
        {
            return; // 取消关闭
        }
        else if (msgBox.clickedButton() == saveBtn)
        {
            // 保存当前文件
            m_currentTabIndex = index;
            if (!on_actionSave_triggered())
            {
                return; // 保存失败取消关闭
            }
        }
        // 点击"不保存"则继续关闭流程
    }

    // 移除标签页
    m_tabWidget->removeTab(index);
    delete info.editor;
    m_tabInfos.remove(index);

    // 如果关闭的是当前标签页，更新当前索引
    if (m_currentTabIndex >= index)
    {
        m_currentTabIndex--;
    }

    // 如果没有标签页了，创建一个新的
    if (m_tabInfos.isEmpty())
    {
        on_actionNew_triggered();
    }
}

// 更新标签页标题
void MainWindow::updateTabTitle(int index)
{
    if (index < 0 || index >= m_tabInfos.size())
        return;

    FileTabInfo &info = m_tabInfos[index];
    QString title = info.displayName + (info.isSaved ? "" : "*");
    m_tabWidget->setTabText(index, title);
}

// 编译操作处理
void MainWindow::on_actionCompile_triggered()
{
    Editor *editor = currentEditor();
    if (!editor)
        return;

    // 添加编译分隔线
    ui->outputTextEdit->appendPlainText("\n--- 开始编译 ---");
    statusBar()->showMessage("编译中...");

    // 获取并编译当前代码
    QString code = editor->getCodeText();
    m_compiler->compile(code);
}

// 获取当前活动的编辑器
Editor *MainWindow::currentEditor() const
{
    if (m_currentTabIndex >= 0 && m_currentTabIndex < m_tabInfos.size())
    {
        return m_tabInfos[m_currentTabIndex].editor;
    }
    return nullptr;
}

// 运行操作处理
void MainWindow::on_actionRun_triggered()
{
    Editor *editor = currentEditor();
    if (!editor)
        return;

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

    // 获取默认字体
    QFont defaultFont = getDefaultEditorFont();

    // 创建新编辑器
    Editor *editor = new Editor();
    // 设置初始示例代码
    QString initialCode =
        "#include <stdio.h>\n\n"
        "int main() {\n"
        "    printf(\"Hello, World!\\n\");\n"
        "    return 0;\n"
        "}";
    editor->setPlainText(initialCode);
    editor->setOriginalText(initialCode);

    // 确保应用默认字体
    editor->setEditorFont(defaultFont);

    // 连接编辑器内容变化信号
    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor]()
            {
        // 直接调用处理函数，但传递正确的编辑器
        for (int i = 0; i < m_tabInfos.size(); ++i) {
            if (m_tabInfos[i].editor == editor) {
                FileTabInfo &info = m_tabInfos[i];
                if (info.isSaved) {
                    info.isSaved = false;
                    updateTabTitle(i);
                    if (i == m_currentTabIndex) {
                        setWindowTitle("TinyIDE - " + info.displayName + "*");
                    }
                }
                break;
            }
        } });

    // 添加到标签页
    int newIndex = m_tabWidget->addTab(editor, "未命名");

    // 存储标签页信息
    FileTabInfo info;
    info.editor = editor;
    info.filePath = "";
    info.isSaved = true;
    info.displayName = "未命名";
    m_tabInfos.append(info);

    // 延迟调用，确保编辑器能够找到主窗口
    //QTimer::singleShot(100, this, [editor]()
    //                  { editor->findActionsFromMainWindow(); });

    // 切换到新标签页
    m_tabWidget->setCurrentIndex(newIndex);
    m_currentTabIndex = newIndex;

    // 更新窗口标题
    setWindowTitle("TinyIDE - 未命名");
}

// 打开文件处理
void MainWindow::on_actionOpen_triggered()
{
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

    // 获取默认字体
    QFont defaultFont = getDefaultEditorFont();

    // 创建新编辑器
    Editor *editor = new Editor();

    // 确保新编辑器使用默认字体
    editor->setEditorFont(defaultFont);

    QTextStream in(&file);
    QString content = in.readAll();
    // 检查行数
    if (content.count('\n') + 1 > 2000) {
        QMessageBox::critical(this, "错误", "文件行数超过2000行，无法打开！");
        file.close();
        delete editor; // 清理已创建的编辑器
        return;
    }
    file.close();

    // 设置编辑器内容
    editor->setPlainText(content);
    editor->setOriginalText(content);

    // 连接编辑器内容变化信号
    connect(editor, &QPlainTextEdit::textChanged,
            this, &MainWindow::onEditorTextChanged);

    // 获取文件名
    QString fileName = QFileInfo(filePath).fileName();

    // 添加到标签页
    int newIndex = m_tabWidget->addTab(editor, fileName);

    // 存储标签页信息
    FileTabInfo info;
    info.editor = editor;
    info.filePath = filePath;
    info.isSaved = true;
    info.displayName = fileName;
    m_tabInfos.append(info);

    // 延迟调用，确保编辑器能够找到主窗口
    //QTimer::singleShot(100, this, [editor]()
    //                   { editor->findActionsFromMainWindow(); });

    // 切换到新标签页
    m_tabWidget->setCurrentIndex(newIndex);
    m_currentTabIndex = newIndex;

    // 更新窗口标题
    setWindowTitle("TinyIDE - " + fileName);
}

// 保存文件处理
bool MainWindow::on_actionSave_triggered()
{
    if (m_currentTabIndex < 0 || m_currentTabIndex >= m_tabInfos.size())
        return false;

    FileTabInfo &info = m_tabInfos[m_currentTabIndex];

    // 无路径时调用另存为
    if (info.filePath.isEmpty())
    {
        return on_actionSaveAs_triggered();
    }

    // 写入文件
    QFile file(info.filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "错误", "无法保存文件: " + file.errorString());
        return false;
    }

    QTextStream out(&file);
    out << info.editor->getCodeText();
    file.close();
    info.editor->setOriginalText(info.editor->getCodeText());

    // 更新保存状态
    info.isSaved = true;
    updateTabTitle(m_currentTabIndex);
    statusBar()->showMessage("文件已保存: " + info.filePath);

    // 更新窗口标题
    setWindowTitle("TinyIDE - " + info.displayName);

    return true;
}

// 另存为文件处理
bool MainWindow::on_actionSaveAs_triggered()
{
    if (m_currentTabIndex < 0 || m_currentTabIndex >= m_tabInfos.size())
        return false;

    FileTabInfo &info = m_tabInfos[m_currentTabIndex];

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
    info.filePath = filePath;
    info.displayName = QFileInfo(filePath).fileName();
    bool result = on_actionSave_triggered();

    // 更新标签页标题
    updateTabTitle(m_currentTabIndex);

    return result;
}

// 关闭文件处理
void MainWindow::on_actionClose_triggered()
{
    if (m_currentTabIndex < 0 || m_currentTabIndex >= m_tabInfos.size())
        return;

    // 触发关闭当前标签页
    onTabCloseRequested(m_currentTabIndex);
}

// 退出应用处理
void MainWindow::on_actionExit_triggered()
{
    // 检查所有标签页是否有未保存的更改
    for (int i = 0; i < m_tabInfos.size(); ++i)
    {
        if (!m_tabInfos[i].isSaved)
        {
            QString fileName = m_tabInfos[i].filePath.isEmpty() ? "未命名文件" : QFileInfo(m_tabInfos[i].filePath).fileName();
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("保存提示");
            msgBox.setText(QString("%1 已修改，是否保存？").arg(fileName));

            // 使用兼容的 ButtonRole 枚举值
            QPushButton *saveBtn = msgBox.addButton("保存", QMessageBox::ButtonRole::YesRole);
            QPushButton *discardBtn = msgBox.addButton("不保存", QMessageBox::ButtonRole::NoRole);
            QPushButton *cancelBtn = msgBox.addButton("取消", QMessageBox::ButtonRole::RejectRole);
            Q_UNUSED(discardBtn); // 标记变量为有意未使用，消除警告
            msgBox.setDefaultButton(saveBtn);
            msgBox.exec();

            if (msgBox.clickedButton() == cancelBtn)
            {
                return; // 取消退出
            }
            else if (msgBox.clickedButton() == saveBtn)
            {
                m_currentTabIndex = i;
                m_tabWidget->setCurrentIndex(i);
                if (!on_actionSave_triggered())
                {
                    return; // 保存失败取消退出
                }
            }
            // 点击"不保存"则继续检查下一个标签页
        }
    }

    // 退出应用
    qApp->quit();
}

// 停止程序运行
void MainWindow::on_actionStop_triggered()
{
    // 调用编译器的停止方法
    m_compiler->stopProgram();
    statusBar()->showMessage("程序已停止");
}

// 编辑器内容变化处理
void MainWindow::onEditorTextChanged()
{
    // 确定是哪个编辑器发出的信号
    Editor *senderEditor = qobject_cast<Editor *>(sender());
    if (!senderEditor)
        return;

    // 找到对应的标签页
    for (int i = 0; i < m_tabInfos.size(); ++i)
    {
        if (m_tabInfos[i].editor == senderEditor)
        {
            FileTabInfo &info = m_tabInfos[i];

            // 标记文件为未保存状态
            if (info.isSaved)
            {
                info.isSaved = false;
                updateTabTitle(i);

                // 如果是当前标签页，更新窗口标题
                if (i == m_currentTabIndex)
                {
                    setWindowTitle("TinyIDE - " + info.displayName + "*");
                }
            }
            break;
        }
    }
}

// 发送输入到运行中的程序
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
