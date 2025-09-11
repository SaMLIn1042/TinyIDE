#include "editor.h"
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QInputDialog>
#include <QTextCursor>
#include <QTextDocument>
#include <QFontDialog> // 新增：字体对话框头文件
#include <QStatusBar>
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>

// 初始化文本编辑器和动作指针
Editor::Editor(QWidget *parent) : QPlainTextEdit(parent),
                                  undoAction(nullptr), cutAction(nullptr),
                                  copyAction(nullptr), pasteAction(nullptr),
                                  findAction(nullptr), replaceAction(nullptr), insertAction(nullptr), fontAction(nullptr),
                                  lineNumberArea(new LineNumberArea(this)) // 新增：初始化fontAction

{
    setUndoRedoEnabled(true);    // 启用撤销/重做功能
    setTabReplace(true, 4);      // 初始化Tab替换为4个空格
    findActionsFromMainWindow(); // 从主窗口查找关联动作
    setupConnections();          // 建立信号槽连接
    updateActionStates();        // 更新动作状态

    // 初始化原始文本
    m_originalText = toPlainText();

    // 连接信号槽用于行号显示和当前行高亮
    connect(this, &Editor::blockCountChanged, this, &Editor::updateLineNumberAreaWidth);
    connect(this, &Editor::updateRequest, this, &Editor::updateLineNumberArea);
    connect(this, &Editor::cursorPositionChanged, this, &Editor::highlightCurrentLine);
    connect(this, &Editor::textChanged, this, &Editor::onTextChanged);

    // 初始更新行号区域宽度
    updateLineNumberAreaWidth(0);
    // 高亮当前行
    highlightCurrentLine();
    // 初始检查新增行
    highlightNewLines();
}
// 获取行号区域宽度
int Editor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        ++digits;
    }

    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

// 绘制行号区域
void Editor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    // 绘制行号
    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);

            // 如果是新增行，用不同颜色显示行号
            if (m_newLineNumbers.contains(blockNumber + 1))
            {
                painter.setPen(Qt::blue);
            }

            painter.drawText(0, top, lineNumberArea->width() - 3, fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// 处理文本变化，跟踪新增内容
void Editor::onTextChanged()
{
    highlightNewLines();
}

// 高亮显示新增行
void Editor::highlightNewLines()
{
    m_newLineNumbers.clear();

    QString currentText = toPlainText();
    QStringList originalLines = m_originalText.split('\n');
    QStringList currentLines = currentText.split('\n');

    // 找出新增的行
    int maxLines = qMax(originalLines.size(), currentLines.size());
    for (int i = 0; i < maxLines; ++i)
    {
        if (i >= originalLines.size())
        {
            // 新增的行
            m_newLineNumbers.insert(i + 1);
        }
        else if (i < currentLines.size() && currentLines[i] != originalLines[i])
        {
            // 修改过的行
            m_newLineNumbers.insert(i + 1);
        }
    }

    // 更新显示
    lineNumberArea->update();
    highlightCurrentLine();
}

// 调整行号区域宽度
void Editor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

// 更新行号区域
void Editor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

// 处理窗口大小变化
void Editor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// 高亮当前行
void Editor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;

        // 当前行高亮颜色
        QColor lineColor = QColor(Qt::yellow).lighter(160);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);

        // 新增行高亮
        QTextCursor cursor = textCursor();
        QTextBlock block = document()->firstBlock();
        while (block.isValid())
        {
            int lineNum = block.blockNumber() + 1;
            if (m_newLineNumbers.contains(lineNum))
            {
                QTextEdit::ExtraSelection newLineSelection;
                QColor newLineColor = QColor(Qt::green).lighter(180);
                newLineSelection.format.setBackground(newLineColor);
                newLineSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
                newLineSelection.cursor = QTextCursor(block);
                newLineSelection.cursor.clearSelection();
                extraSelections.append(newLineSelection);
            }
            block = block.next();
        }
    }

    setExtraSelections(extraSelections);
}
// 新增：设置原始文本（用于跟踪新增内容）
void Editor::setOriginalText(const QString &text)
{
    m_originalText = text;
    highlightNewLines(); // 重新计算新增行
}

// 获取编辑器中的纯文本内容
QString Editor::getCodeText() const
{
    return toPlainText();
}

// 设置编辑器字体
void Editor::setEditorFont(const QFont &font)
{
    setFont(font);
    // 更新Tab宽度以适应新字体
    setTabReplace(true, 4);
}

// 获取当前编辑器字体
QFont Editor::getEditorFont() const
{
    return font();
}

// 在主窗口中查找并关联编辑动作
void Editor::findActionsFromMainWindow()
{
    // 获取父窗口中的动作对象
    QMainWindow *mainWindow = qobject_cast<QMainWindow *>(window());
    if (!mainWindow)
    {
        qDebug() << "错误：未找到主窗口";
        return;
    }

    // 遍历所有动作并匹配对象名
    QList<QAction *> allActions = mainWindow->findChildren<QAction *>();
    foreach (QAction *action, allActions)
    {
        const QString &objName = action->objectName();
        // 根据对象名关联对应动作
        if (objName == "actionUndo")
        {
            undoAction = action;
        }
        else if (objName == "actionCut")
        {
            cutAction = action;
        }
        else if (objName == "actionCopy")
        {
            copyAction = action;
        }
        else if (objName == "actionPaste")
        {
            pasteAction = action;
        }
        else if (objName == "actionFind")
        {
            findAction = action;
        }
        else if (objName == "actionReplace")
        {
            replaceAction = action;
        }
        else if (objName == "actionInsert")
        {
            insertAction = action;
        }
        else if (objName == "actionFont")
        { // 新增：关联字体设置动作
            fontAction = action;
        }
    }

    // 调试输出动作查找结果
    qDebug() << "动作匹配情况：";
    qDebug() << "actionUndo: " << (undoAction ? "找到" : "未找到");
    qDebug() << "actionCut: " << (cutAction ? "找到" : "未找到");
    qDebug() << "actionCopy: " << (copyAction ? "找到" : "未找到");
    qDebug() << "actionPaste: " << (pasteAction ? "找到" : "未找到");
    qDebug() << "actionFind: " << (findAction ? "找到" : "未找到");
    qDebug() << "actionReplace: " << (replaceAction ? "找到" : "未找到");
    qDebug() << "actionInsert: " << (insertAction ? "找到" : "未找到");
    qDebug() << "actionFont: " << (fontAction ? "找到" : "未找到"); // 新增：字体动作状态
}

// 建立动作与编辑器功能的连接
void Editor::setupConnections()
{
    // 连接撤销动作
    if (undoAction)
    {
        disconnect(undoAction, 0, 0, 0);
        connect(undoAction, &QAction::triggered, this, &Editor::handleUndo);
        connect(this, &QPlainTextEdit::undoAvailable, undoAction, &QAction::setEnabled);
    }

    // 连接剪切动作
    if (cutAction)
    {
        disconnect(cutAction, 0, 0, 0);
        connect(cutAction, &QAction::triggered, this, &Editor::handleCut);
        connect(this, &QPlainTextEdit::copyAvailable, cutAction, &QAction::setEnabled);
    }

    // 连接复制动作
    if (copyAction)
    {
        disconnect(copyAction, 0, 0, 0);
        connect(copyAction, &QAction::triggered, this, &Editor::handleCopy);
        connect(this, &QPlainTextEdit::copyAvailable, copyAction, &QAction::setEnabled);
    }

    // 连接粘贴动作
    if (pasteAction)
    {
        disconnect(pasteAction, 0, 0, 0);
        connect(pasteAction, &QAction::triggered, this, &Editor::handlePaste);
        connect(QApplication::clipboard(), &QClipboard::dataChanged,
                this, &Editor::updatePasteState);
    }

    // 连接查找动作（设置标准快捷键）
    if (findAction)
    {
        disconnect(findAction, 0, 0, 0);
        findAction->setShortcut(QKeySequence::Find); // 设置查找快捷键 (Ctrl+F)
        findAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(findAction, &QAction::triggered, this, &Editor::handleFind);
    }

    // 连接替换动作（设置标准快捷键）
    if (replaceAction)
    {
        disconnect(replaceAction, 0, 0, 0);
        replaceAction->setShortcut(QKeySequence::Replace); // 设置替换快捷键 (Ctrl+H)
        replaceAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(replaceAction, &QAction::triggered, this, &Editor::handleReplace);
    }

    // 连接插入动作
    if (insertAction)
    {
        disconnect(insertAction, 0, 0, 0);
        connect(insertAction, &QAction::triggered, this, &Editor::handleInsert);
    }

    // 新增：连接字体设置动作
    if (fontAction)
    {
        disconnect(fontAction, 0, 0, 0);
        fontAction->setShortcut(QKeySequence::fromString("Ctrl+F12")); // 设置字体快捷键
        fontAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(fontAction, &QAction::triggered, this, &Editor::handleFontSettings);
    }

    connect(this, &QPlainTextEdit::textChanged, this, &Editor::updateActionStates);
    QAction *commentAction = new QAction(this);
    commentAction->setShortcut(QKeySequence("Ctrl+/"));
    connect(commentAction, &QAction::triggered, this, &Editor::handleComment);
    addAction(commentAction);
}

// 更新编辑动作的启用状态
void Editor::updateActionStates()
{
    // 更新撤销动作状态
    if (undoAction)
    {
        undoAction->setEnabled(document()->isUndoAvailable());
    }

    // 根据文本选择状态更新剪切/复制动作
    bool hasSelection = textCursor().hasSelection();
    if (cutAction)
        cutAction->setEnabled(hasSelection);
    if (copyAction)
        copyAction->setEnabled(hasSelection);

    // 更新粘贴动作状态
    updatePasteState();
}

// 更新粘贴动作的启用状态（基于剪贴板内容）
void Editor::updatePasteState()
{
    if (pasteAction)
    {
        pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
    }
}

// 执行撤销操作
void Editor::handleUndo()
{
    undo();
    updateActionStates();
    highlightNewLines(); // 新增：撤销后更新新增行标记
}

// 执行剪切操作
void Editor::handleCut()
{
    cut();
    updateActionStates();
}

// 执行复制操作
void Editor::handleCopy()
{
    copy();
    updateActionStates();
}

// 执行粘贴操作（补充Tab替换逻辑）
void Editor::handlePaste()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();

    // 将粘贴内容中的Tab替换为设置的空格数（使用当前Tab宽度计算空格数）
    int tabWidth = tabStopWidth() / fontMetrics().width(' ');
    text.replace("\t", QString(" ").repeated(tabWidth));

    QTextCursor cursor = textCursor();
    cursor.insertText(text);
    setTextCursor(cursor);
    updateActionStates();
}

// 处理查找功能
void Editor::handleFind()
{
    // 显示查找对话框获取搜索文本
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("查找"),
                                               tr("输入要查找的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || searchText.isEmpty())
        return;

    // 设置查找选项（如大小写敏感）
    QTextDocument::FindFlags flags;
    // flags |= QTextDocument::FindCaseSensitively; // 取消注释启用大小写敏感

    // 从当前光标位置开始查找
    QTextCursor cursor = textCursor();
    cursor = document()->find(searchText, cursor, flags);

    // 处理查找结果
    if (!cursor.isNull())
    {
        setTextCursor(cursor); // 选中找到的文本
    }
    else
    {
        // 未找到则从文档开头重新查找
        cursor = document()->find(searchText, QTextCursor(document()));
        if (!cursor.isNull())
        {
            setTextCursor(cursor);
        }
        else
        {
            qDebug() << "未找到匹配的文本: " << searchText;
        }
    }
}

// 处理替换功能
void Editor::handleReplace()
{
    // 获取要查找的文本
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("替换"),
                                               tr("输入要查找的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || searchText.isEmpty())
        return;

    // 获取替换文本
    QString replaceText = QInputDialog::getText(this, tr("替换"),
                                                tr("替换为:"), QLineEdit::Normal,
                                                "", &ok);
    if (!ok)
        return;

    // 提供替换选项：单个替换或全部替换
    QStringList options;
    options << tr("替换当前匹配") << tr("替换全部匹配");
    QString choice = QInputDialog::getItem(this, tr("替换选项"),
                                           tr("选择操作:"), options, 0, false, &ok);
    if (!ok)
        return;

    // 执行选择的替换操作
    if (choice == options[0])
    {
        replaceCurrent(searchText, replaceText); // 替换当前匹配项
    }
    else
    {
        replaceAll(searchText, replaceText); // 替换所有匹配项
    }
    highlightNewLines(); // 新增：替换后更新新增行标记
}

// 替换当前选中的匹配项
void Editor::replaceCurrent(const QString &searchText, const QString &replaceText)
{
    QTextCursor cursor = document()->find(searchText, textCursor());
    if (!cursor.isNull())
    {
        cursor.insertText(replaceText); // 执行替换
        setTextCursor(cursor);          // 更新光标位置
    }
    else
    {
        qDebug() << "未找到匹配的文本: " << searchText;
    }
}

// 替换所有匹配项
void Editor::replaceAll(const QString &searchText, const QString &replaceText)
{
    QTextCursor cursor(document());
    int count = 0;

    // 遍历文档替换所有匹配项
    while (!(cursor = document()->find(searchText, cursor)).isNull())
    {
        cursor.insertText(replaceText);
        ++count; // 计数替换次数
    }
    qDebug() << "共替换" << count << "处匹配文本";
}

// 处理文本插入功能
void Editor::handleInsert()
{
    // 获取要插入的文本
    bool ok;
    QString insertText = QInputDialog::getText(this, tr("插入文本"),
                                               tr("输入要插入的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || insertText.isEmpty())
        return;

    // 在光标位置插入文本
    QTextCursor cursor = textCursor();
    cursor.insertText(insertText);
    setTextCursor(cursor); // 更新光标位置
}

// 新增：处理字体设置功能
void Editor::handleFontSettings()
{
    bool ok;
    QFont currentFont = getEditorFont(); // 获取当前字体

    // 弹出字体选择对话框
    QFont newFont = QFontDialog::getFont(
        &ok,                 // 用户是否点击确定
        currentFont,         // 当前字体作为初始值
        this,                // 父窗口
        tr("选择编辑器字体") // 对话框标题
    );

    if (ok)
    {
        // 应用新字体
        setEditorFont(newFont);

        // 在状态栏显示字体信息（如果有状态栏）
        QMainWindow *mainWindow = qobject_cast<QMainWindow *>(window());
        if (mainWindow && mainWindow->statusBar())
        {
            mainWindow->statusBar()->showMessage(
                tr("字体已更新: %1 %2pt")
                    .arg(newFont.family())
                    .arg(newFont.pointSize()),
                3000 // 3秒后自动消失
            );
        }

        qDebug() << "字体已更新为: " << newFont.family()
                 << ", 大小: " << newFont.pointSize() << "pt";
    }
}

// 实现Tab替换为空格的核心功能
void Editor::setTabReplace(bool replace, int spaces)
{
    if (replace)
    {
        // 设置Tab键插入对应数量的空格（通过调整Tab宽度实现）
        setTabStopWidth(spaces * fontMetrics().width(' '));
    }
    else
    {
        // 恢复默认Tab行为（8个空格宽度）
        setTabStopWidth(8 * fontMetrics().width(' '));
    }
    // 在setupConnections()函数中添加快捷键连接
}
void Editor::handleComment()
{
    QTextCursor cursor = textCursor();
    bool hasSelection = cursor.hasSelection();

    if (hasSelection)
    {
        // 处理选中区域的注释
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();

        // 移动到选中区域的起始行
        cursor.setPosition(start);
        cursor.movePosition(QTextCursor::StartOfLine);
        int startLine = cursor.position();

        // 移动到选中区域的结束行
        cursor.setPosition(end);
        cursor.movePosition(QTextCursor::EndOfLine);
        int endLine = cursor.position();

        // 选中所有要注释的行
        cursor.setPosition(startLine);
        cursor.setPosition(endLine, QTextCursor::KeepAnchor);
        QString selectedText = cursor.selectedText();
        QStringList lines = selectedText.split("\n");

        // 检查是否已经是注释
        bool isCommented = lines.first().trimmed().startsWith("//");

        QString processedText;
        if (isCommented)
        {
            // 取消注释
            foreach (QString line, lines)
            {
                processedText += line.replace(QRegExp("^\\s*//"), "") + "\n";
            }
        }
        else
        {
            // 添加注释
            foreach (QString line, lines)
            {
                processedText += "//" + line + "\n";
            }
        }

        // 替换选中的文本
        cursor.insertText(processedText.left(processedText.length() - 1)); // 移除最后一个换行
        setTextCursor(cursor);
    }
    else
    {
        // 没有选中区域，注释当前行
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        if (line.trimmed().startsWith("//"))
        {
            // 取消注释
            line = line.replace(QRegExp("^\\s*//"), "");
        }
        else
        {
            // 添加注释
            line = "//" + line;
        }

        cursor.insertText(line);
    }

    highlightNewLines(); // 更新新增行标记
}
