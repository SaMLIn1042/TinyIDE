#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScrollBar>
#include <QStatusBar>
#include <QDebug>
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 初始化编辑器（使用UI中已有的editor组件）
    m_editor = findChild<Editor*>("editor");
    if(!m_editor) {
        qDebug() << "警告：未找到editor组件，使用默认位置";
        m_editor = new Editor(this);
        setCentralWidget(m_editor);
    }

    // 初始化编译器
    m_compiler = new Compiler(this);

    // 连接编译器信号与主窗口槽函数
    connect(m_compiler, &Compiler::compileFinished,
            this, &MainWindow::onCompileFinished);
    connect(m_compiler, &Compiler::runFinished,
            this, &MainWindow::onRunFinished);
    connect(m_compiler, &Compiler::runOutput,
            this, &MainWindow::handleRunOutput);

    // -------------------------- 复用现有“编辑”菜单，添加功能 --------------------------
    // 查找现有“编辑”菜单（优先通过objectName，其次通过标题）
    QMenu *editMenu = menuBar()->findChild<QMenu*>("menu_Edit");  // 假设UI中编辑菜单的objectName为menu_Edit
    if (!editMenu) {
        // 若未找到，按标题查找（支持中文翻译）
        foreach (QMenu *menu, menuBar()->findChildren<QMenu*>()) {
            if (menu->title() == tr("编辑(&E)")) {  // 匹配“编辑”菜单标题
                editMenu = menu;
                break;
            }
        }
    }
    // 极端情况：若仍未找到，创建一个（避免崩溃，实际应确保UI中已有）
    if (!editMenu) {
        editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    }

    // 向现有编辑菜单添加功能（按逻辑顺序插入）
    // 1. 撤销
    QAction *undoAction = new QAction(tr("撤销(&U)"), this);
    undoAction->setStatusTip(tr("撤销上一步操作"));
    // 无撤销历史时禁用按钮
    connect(m_editor, &QPlainTextEdit::undoAvailable, undoAction, &QAction::setEnabled);
    connect(undoAction, &QAction::triggered, this, &MainWindow::on_actionUndo_triggered);
    // 插入到菜单最前面


    // 2. 恢复
    QAction *redoAction = new QAction(tr("恢复(&R)"), this);
    redoAction->setShortcut(QKeySequence::Redo);  // 快捷键：Ctrl+Y
    redoAction->setStatusTip(tr("恢复上一步撤销的操作"));
    // 无恢复历史时禁用按钮
    connect(m_editor, &QPlainTextEdit::redoAvailable, redoAction, &QAction::setEnabled);
    connect(redoAction, &QAction::triggered, this, &MainWindow::on_actionRedo_triggered);
    // 插入到撤销功能后面

    // 3. 剪切
    QAction *cutAction = new QAction(tr("剪切(&T)"), this);
    cutAction->setShortcut(QKeySequence::Cut);  // 快捷键：Ctrl+X
    cutAction->setStatusTip(tr("剪切选中文本到剪贴板"));
    // 无选中文本时禁用按钮
    connect(m_editor, &QPlainTextEdit::copyAvailable, cutAction, &QAction::setEnabled);
    connect(cutAction, &QAction::triggered, this, &MainWindow::on_actionCut_triggered);
    // 插入到分隔线后面

    // 4. 复制
    QAction *copyAction = new QAction(tr("复制(&C)"), this);
    copyAction->setShortcut(QKeySequence::Copy);  // 快捷键：Ctrl+C
    copyAction->setStatusTip(tr("复制选中文本到剪贴板"));
    // 无选中文本时禁用按钮
    connect(m_editor, &QPlainTextEdit::copyAvailable, copyAction, &QAction::setEnabled);
    connect(copyAction, &QAction::triggered, this, &MainWindow::on_actionCopy_triggered);
    // 插入到剪切功能后面

    // 5. 粘贴
    QAction *pasteAction = new QAction(tr("粘贴(&P)"), this);
    pasteAction->setShortcut(QKeySequence::Paste);  // 快捷键：Ctrl+V
    pasteAction->setStatusTip(tr("粘贴剪贴板内容到当前光标位置"));
    // 剪贴板为空时禁用按钮
    QClipboard *clipboard = QApplication::clipboard();
    pasteAction->setEnabled(!clipboard->text().isEmpty());  // 初始状态
    connect(clipboard, &QClipboard::dataChanged, [=]() {
        pasteAction->setEnabled(!clipboard->text().isEmpty());  // 内容变化时更新状态
    });
    connect(pasteAction, &QAction::triggered, this, &MainWindow::on_actionPaste_triggered);
    // 插入到复制功能后面

    //查找功能
    ui->actionFind->setShortcut(QKeySequence::Find);
    ui->actionFind->setShortcutContext(Qt::WindowShortcut);
    connect(ui->actionFind, &QAction::triggered, this, &MainWindow::on_actionFind_triggered);

    // 绑定快捷键 F3 / Shift+F3
    QAction *findNextAction = new QAction(this);
    findNextAction->setShortcut(QKeySequence(Qt::Key_F3));
    connect(findNextAction, &QAction::triggered, this, &MainWindow::findNext);
    addAction(findNextAction);

    QAction *findPrevAction = new QAction(this);
    findPrevAction->setShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_F3));
    connect(findPrevAction, &QAction::triggered, this, &MainWindow::findPrevious);
    addAction(findPrevAction);


    //替换功能
    ui->actionReplace->setShortcut(QKeySequence::Replace);  // 通常是 Ctrl+H
    ui->actionReplace->setShortcutContext(Qt::WindowShortcut);
    connect(ui->actionReplace, &QAction::triggered,this, &MainWindow::on_actionReplace_triggered);

    //插入功能
    ui->actionInsert->setShortcutContext(Qt::WindowShortcut); // 可选：确保快捷键在窗口范围有效
    connect(ui->actionInsert, &QAction::triggered,this, &MainWindow::on_actionInsert_triggered);

    //清除高亮
    QAction *clearHighlightAction = new QAction(tr("清除高亮"), this);
    clearHighlightAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L)); // 例：Ctrl+L
    clearHighlightAction->setStatusTip(tr("清除所有文本高亮"));
    connect(clearHighlightAction, &QAction::triggered, this, &MainWindow::clearHighlights);
    editMenu->addAction(clearHighlightAction);

    //高亮选中内容
    QAction *highlightAction = new QAction(tr("高亮选中"), this);
    highlightAction->setShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_H)); // 可选快捷键
    highlightAction->setStatusTip(tr("高亮当前选中的文本"));
    connect(highlightAction, &QAction::triggered, this, &MainWindow::highlightSelection);
    editMenu->addAction(highlightAction);

}

MainWindow::~MainWindow() {
    delete ui;
    delete m_compiler;  // 释放编译器资源

}

// -------------------------- 编译功能 --------------------------
void MainWindow::on_actionCompile_triggered() {
    // 清空输出区域（使用UI中实际的输出控件名）
    ui->outputTextEdit->clear();
    ui->outputTextEdit->appendPlainText("=== 开始编译代码 ===");
    statusBar()->showMessage("编译中...");

    // 获取编辑器中的代码
    QString code = m_editor->getCodeText();
    if (code.isEmpty()) {
        ui->outputTextEdit->appendPlainText("错误：代码为空，无法编译！");
        statusBar()->showMessage("编译失败（空代码）", 2000);
        return;
    }

    // 触发编译
    m_compiler->compile(code);
}

// -------------------------- 运行功能 --------------------------
void MainWindow::on_actionRun_triggered() {
    // 判断编译是否成功（使用Compiler类的isCompileSuccess()函数）
    if (!m_compiler->isCompileSuccess()) {
        ui->outputTextEdit->appendPlainText("\n=== 运行失败 ===");
        ui->outputTextEdit->appendPlainText("错误：请先成功编译代码！");
        statusBar()->showMessage("运行失败（未编译）", 2000);
        return;
    }

    // 编译成功，启动运行
    ui->outputTextEdit->appendPlainText("\n=== 开始运行程序 ===");
    statusBar()->showMessage("运行中...");
    m_compiler->runProgram();
}

// -------------------------- 编译完成回调 --------------------------
void MainWindow::onCompileFinished(bool success, const QString &output) {
    ui->outputTextEdit->appendPlainText("\n=== 编译结果 ===");
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到输出底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());

    // 更新状态栏
    statusBar()->showMessage(success ? "编译成功" : "编译失败", 2000);
}

// -------------------------- 运行完成回调 --------------------------
void MainWindow::onRunFinished(bool success, const QString &output) {
    ui->outputTextEdit->appendPlainText("\n=== 运行结果 ===");
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到输出底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());

    // 更新状态栏
    statusBar()->showMessage(success ? "运行完成" : "运行失败", 2000);
}

// -------------------------- 程序实时输出回调 --------------------------
void MainWindow::handleRunOutput(const QString &output) {
    ui->outputTextEdit->appendPlainText(output);

    // 滚动到输出底部
    QScrollBar *scrollbar = ui->outputTextEdit->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

// -------------------------- 编辑功能实现 --------------------------
void MainWindow::on_actionUndo_triggered() {
    m_editor->undo();
    statusBar()->showMessage("已撤销上一步操作", 2000);
}

void MainWindow::on_actionRedo_triggered() {
    m_editor->redo();
    statusBar()->showMessage("已恢复上一步操作", 2000);
}

void MainWindow::on_actionCut_triggered() {
    m_editor->cut();
    statusBar()->showMessage("已剪切选中文本", 2000);
}

void MainWindow::on_actionCopy_triggered() {
    m_editor->copy();
    statusBar()->showMessage("已复制选中文本到剪贴板", 2000);
}

void MainWindow::on_actionPaste_triggered() {
    m_editor->paste();
    statusBar()->showMessage("已粘贴剪贴板内容", 2000);
}

void MainWindow::on_actionFind_triggered()
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("查找"),
                                         tr("输入要查找的内容:"), QLineEdit::Normal,
                                         m_lastFindText, &ok);
    if (ok && !text.isEmpty()) {
        m_lastFindText = text;  // 保存关键字
        highlightAllMatches(m_lastFindText);

        // 尝试从当前光标开始查找
        if (!m_editor->find(m_lastFindText)) {
            QTextCursor cursor = m_editor->textCursor();
            cursor.movePosition(QTextCursor::Start);
            m_editor->setTextCursor(cursor);
            if (!m_editor->find(m_lastFindText)) {
                QMessageBox::information(this, tr("查找结果"), tr("未找到匹配内容。"));
            }
        }
    }
}

void MainWindow::findNext()
{
    if (m_lastFindText.isEmpty()) {
        on_actionFind_triggered();  // 第一次没有关键字，弹窗输入
        return;
    }
    if (!m_editor->find(m_lastFindText)) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(cursor);
        m_editor->find(m_lastFindText);
    }
}

void MainWindow::findPrevious()
{
    if (m_lastFindText.isEmpty()) {
        on_actionFind_triggered();
        return;
    }
    if (!m_editor->find(m_lastFindText, QTextDocument::FindBackward)) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_editor->setTextCursor(cursor);
        m_editor->find(m_lastFindText, QTextDocument::FindBackward);
    }
}

void MainWindow::highlightAllMatches(const QString &text)
{
    QList<QTextEdit::ExtraSelection> extras;

    if (!text.isEmpty()) {
        QTextCursor cursor = m_editor->document()->find(text);
        QTextCharFormat format;
        format.setBackground(Qt::yellow);
        format.setForeground(Qt::black);

        while (!cursor.isNull()) {
            QTextEdit::ExtraSelection selection;
            selection.cursor = cursor;
            selection.format = format;
            extras.append(selection);
            cursor = m_editor->document()->find(text, cursor);
        }
    }

    m_editor->setExtraSelections(extras);
}


void MainWindow::on_actionReplace_triggered()
{
    bool ok;
    // 1. 获取要查找的内容
    QString findText = QInputDialog::getText(this, tr("查找"),
                                             tr("输入要查找的文本:"), QLineEdit::Normal,
                                             QString(), &ok);
    if (!ok || findText.isEmpty()) return;

    // 2. 获取要替换成的内容
    QString replaceText = QInputDialog::getText(this, tr("替换为"),
                                                tr("输入要替换成的文本:"), QLineEdit::Normal,
                                                QString(), &ok);
    if (!ok) return;

    // 3. 询问用户要替换一个还是替换所有
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("替换选项"),
                                                              tr("替换一个匹配还是所有匹配？\n一个请选Yes，全部请选No。"),
                                                              QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                                              QMessageBox::Yes);
    // Yes -> 替换一个, No -> 替换所有, Cancel -> 退出
    if (reply == QMessageBox::Cancel) return;

    QTextCursor cursor = m_editor->textCursor();

    if (reply == QMessageBox::Yes) {
        // 替换一个：从当前光标位置开始查找
        if (m_editor->find(findText)) {
            cursor = m_editor->textCursor();
            cursor.insertText(replaceText);
            m_editor->setTextCursor(cursor);
            statusBar()->showMessage(tr("已替换一个匹配"), 2000);
        } else {
            QMessageBox::information(this, tr("结果"), tr("未找到匹配文本"));
        }
    }
    else if (reply == QMessageBox::No) {
        // 替换所有：从头开始查找
        cursor.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(cursor);

        int count = 0;
        while (m_editor->find(findText)) {
            cursor = m_editor->textCursor();
            cursor.insertText(replaceText);
            count++;
        }
        QMessageBox::information(this, tr("结果"),
                                 tr("已替换所有匹配，共 %1 处").arg(count));
    }
}

void MainWindow::on_actionInsert_triggered()
{
    bool ok;
    // 弹出对话框让用户输入要插入的文本
    QString text = QInputDialog::getText(this, tr("插入文本"),
                                         tr("输入要插入的文本:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (!ok || text.isEmpty())
    {
        return; // 用户取消或没输入
    }
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(text);
    m_editor->setTextCursor(cursor);
    m_editor->setFocus(); // 插入后把焦点回到编辑器
    statusBar()->showMessage(tr("已插入文本"), 2000);
}

void MainWindow::clearHighlights()
{
    QList<QTextEdit::ExtraSelection> empty; // 空列表
    m_editor->setExtraSelections(empty);    // 设置为空即可取消高亮
}

void MainWindow::highlightSelection()
{
    QList<QTextEdit::ExtraSelection> extras;

    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;

        QTextCharFormat format;
        format.setBackground(Qt::yellow);   // 背景颜色
        format.setForeground(Qt::black);    // 前景颜色（可选）
        selection.format = format;

        extras.append(selection);
    }

    m_editor->setExtraSelections(extras);
}
