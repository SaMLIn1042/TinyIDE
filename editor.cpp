#include "editor.h"
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QInputDialog>
#include <QTextCursor>
#include <QTextDocument>
#include <QFontDialog>
#include <QStatusBar>
#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QTranslator>
#include <QLibraryInfo>

// 编辑器构造函数，初始化界面和功能
Editor::Editor(QWidget *parent) : QPlainTextEdit(parent),
                                  undoAction(nullptr), cutAction(nullptr),
                                  copyAction(nullptr), pasteAction(nullptr),
                                  findAction(nullptr), replaceAction(nullptr),
                                  insertAction(nullptr), fontAction(nullptr),
                                  lineNumberArea(new LineNumberArea(this))
{
    // 加载中文翻译支持
    loadChineseTranslation();

    setUndoRedoEnabled(true);    // 启用撤销重做功能

//    // 设置默认字体 - 使用更适合代码编辑的等宽字体
//    QFont defaultFont;

//    // 更可靠的字体检测方法
//    QStringList preferredFonts = {
//        "Consolas",
//        "Source Code Pro",
//        "Monaco",
//        "Courier New",
//        "Monospace"  // 通用等宽字体
//    };

//    // 检查系统是否有首选字体
//    QFontDatabase fontDb;
//    QString selectedFont = "Monospace"; // 默认回退

//    for (const QString &fontName : preferredFonts) {
//        if (fontDb.families().contains(fontName)) {
//            selectedFont = fontName;
//            qDebug() << "使用字体:" << fontName;
//            break;
//        }
//    }

//    defaultFont.setFamily(selectedFont);
//    defaultFont.setPointSize(10);
//    defaultFont.setStyleHint(QFont::TypeWriter); // 明确指定为等宽字体

//    setFont(defaultFont);

//    // 调试输出当前使用的字体信息
//    qDebug() << "编辑器字体设置为:" << font().family()
//             << "大小:" << font().pointSize()
//             << "是否为等宽字体:" << QFontInfo(font()).fixedPitch();

    setTabReplace(true, 4);      // 设置Tab键替换为4个空格
    findActionsFromMainWindow(); // 从主窗口查找关联动作
    setupConnections();          // 建立信号槽连接
    updateActionStates();        // 更新动作状态

    // 初始化原始文本（用于跟踪新增内容）
    m_originalText = toPlainText();

    // 设置成对符号映射（用于自动补全）
    m_matchingPairs.insert('(', ')');
    m_matchingPairs.insert('{', '}');
    m_matchingPairs.insert('[', ']');
    m_matchingPairs.insert('<', '>');
    m_matchingPairs.insert('\'', '\'');
    m_matchingPairs.insert('"', '"');

    // 连接信号槽用于行号显示和当前行高亮
    connect(this, &Editor::blockCountChanged, this, &Editor::updateLineNumberAreaWidth);
    connect(this, &Editor::updateRequest, this, &Editor::updateLineNumberArea);
    connect(this, &Editor::cursorPositionChanged, this, &Editor::highlightCurrentLine);
    connect(this, &Editor::textChanged, this, &Editor::onTextChanged);
    connect(this, &Editor::cursorPositionChanged, this, &Editor::highlightMatchingBracket);

    // 初始更新行号区域宽度
    updateLineNumberAreaWidth(0);
    // 高亮当前行
    highlightCurrentLine();
    // 初始检查新增行
    highlightNewLines();
    // 初始化语法高亮器（添加在这里）
    highlighter = new EditorSyntaxHighlighter(document());
}

// 获取当前行的缩进级别
int Editor::getIndentationLevel() const
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString line = cursor.selectedText();

    int level = 0;
    int tabWidth = 4; // 假设使用4个空格作为缩进单位
    int spaceCount = 0;

    foreach (QChar c, line) {
        if (c == ' ') {
            spaceCount++;
            if (spaceCount % tabWidth == 0) {
                level++;
            }
        } else if (c == '\t') {
            level++;
            spaceCount = 0;
        } else {
            break;
        }
    }

    return level;
}

// 计算当前应该使用的缩进字符串
QString Editor::calculateIndentation() const
{
    QTextCursor cursor = textCursor();

    // 获取当前行文本（已移除未使用的lineNumber变量）
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString currentLine = cursor.selectedText();

    // 基础缩进级别（当前行的缩进）
    int indentLevel = getIndentationLevel();

    // 如果当前行包含{，则下一行缩进级别+1
    if (currentLine.contains('{') && !currentLine.contains('}')) {
        indentLevel++;
    }

    // 创建缩进字符串（使用空格）
    int tabWidth = 4;
    return QString(tabWidth * indentLevel, ' ');
}

// 加载Qt中文翻译文件
void Editor::loadChineseTranslation()
{
    // 为Qt标准组件加载中文翻译
    QTranslator *qtTranslator = new QTranslator(qApp);
    if (qtTranslator->load("qt_zh_CN.qm", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    {
        qApp->installTranslator(qtTranslator);
    }

    QTranslator *qtBaseTranslator = new QTranslator(qApp);
    if (qtBaseTranslator->load("qtbase_zh_CN.qm", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    {
        qApp->installTranslator(qtBaseTranslator);
    }
}

// 处理按键事件，实现成对符号自动补全和自动缩进
void Editor::keyPressEvent(QKeyEvent *event)
{
    // 处理回车键 - 自动缩进
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        // 获取当前缩进
        QString indent = calculateIndentation();

        // 插入换行和缩进
        QTextCursor cursor = textCursor();
        cursor.insertText("\n" + indent);
        setTextCursor(cursor);

        event->accept();
        return;
    }
    // 处理右花括号 - 自动减少缩进
    else if (event->key() == Qt::Key_BraceRight)
    {
        // 获取当前缩进级别并减1
        int indentLevel = getIndentationLevel();
        if (indentLevel > 0)
        {
            indentLevel--;
        }

        // 创建缩进字符串
        int tabWidth = tabStopWidth() / fontMetrics().width(' '); // 使用当前Tab宽度设置
        QString indent = QString(tabWidth * indentLevel, ' ');

        // 插入右花括号
        QTextCursor cursor = textCursor();
        cursor.insertText("}");

        // 如果是在行首，添加适当的缩进
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();
        if (line.trimmed() == "}")
        {
            cursor.removeSelectedText();
            cursor.insertText(indent + "}");
        }

        setTextCursor(cursor);
        event->accept();
        return;
    }
    // 处理退格键（删除成对符号）
    else if (event->key() == Qt::Key_Backspace)
    {
        QTextCursor cursor = textCursor();

        // 有选定时直接调用父类处理
        if (cursor.hasSelection())
        {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        // 检查是否为成对符号并同时删除
        int pos = cursor.position();
        if (pos > 0) {
            cursor.setPosition(pos - 1);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            QString leftChar = cursor.selectedText();

            if (!leftChar.isEmpty() && m_matchingPairs.contains(leftChar[0])) {
                QChar rightChar = m_matchingPairs[leftChar[0]];

                // 检查右侧是否有匹配的符号
                if (pos < document()->characterCount()) {
                    cursor.setPosition(pos);
                    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                    QString actualRightChar = cursor.selectedText();

                    if (actualRightChar[0] == rightChar) {
                        // 同时删除左右符号
                        cursor.setPosition(pos - 1);
                        cursor.deleteChar();
                        cursor.deleteChar();
                        event->accept();
                        return;
                    }
                }
            }
        }
    }

    // 处理普通字符输入（成对符号自动补全）
    QString inputText = event->text();
    if (!inputText.isEmpty())
    {
        QChar inputChar = inputText.at(0);

        // 检查是否是需要自动补全的左符号
        if (m_matchingPairs.contains(inputChar) &&
            // 排除单引号和双引号在选中内容时的自动补全
            !( (inputChar == '\'' || inputChar == '"') && textCursor().hasSelection() ))
        {
            QChar matchingChar = m_matchingPairs[inputChar]; // 获取匹配的右符号

            // 插入左符号和右符号
            QTextCursor cursor = textCursor();
            int originalPos = cursor.position();

            // 如果是引号且已有选中内容，包裹选中内容
            if ((inputChar == '\'' || inputChar == '"') && cursor.hasSelection()) {
                QString selectedText = cursor.selectedText();
                cursor.removeSelectedText();
                cursor.insertText(inputChar + selectedText + matchingChar);
                cursor.setPosition(originalPos + selectedText.length() + 2);
            } else {
                // 普通符号补全
                cursor.insertText(inputChar);
                cursor.insertText(matchingChar);
                cursor.setPosition(originalPos + 1);
            }

            setTextCursor(cursor);
            event->accept();
            return;
        }
    }

    // 其他情况调用父类方法处理
    QPlainTextEdit::keyPressEvent(event);

    // 按键后更新括号高亮
    highlightMatchingBracket();
}


// 计算行号区域宽度
int Editor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());

    // 计算最大行号所需的位数
    while (max >= 10)
    {
        max /= 10;
        ++digits;
    }

    // 计算宽度（包含边距）
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

// 绘制行号区域
void Editor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray); // 填充背景

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    // 绘制可见行的行号
    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);

            // 新增行用红色显示行号
            if (m_newLineNumbers.contains(blockNumber + 1))
            {
                painter.setPen(Qt::red);
            }

            // 绘制行号文本
            painter.drawText(0, top, lineNumberArea->width() - 3,
                             fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// 文本变化时触发，更新新增行标记
void Editor::onTextChanged()
{
    highlightNewLines();
}

// 高亮显示新增行（与原始文本对比）
void Editor::highlightNewLines()
{
    m_newLineNumbers.clear();

    QString currentText = toPlainText();
    QStringList originalLines = m_originalText.split('\n');
    QStringList currentLines = currentText.split('\n');

    // 使用最长公共子序列算法确定新增行
    int m = originalLines.size();
    int n = currentLines.size();
    QVector<QVector<int>> lcs(m + 1, QVector<int>(n + 1, 0));

    // 计算LCS
    for (int i = 1; i <= m; ++i)
    {
        for (int j = 1; j <= n; ++j)
        {
            if (originalLines[i - 1] == currentLines[j - 1])
            {
                lcs[i][j] = lcs[i - 1][j - 1] + 1;
            }
            else
            {
                lcs[i][j] = qMax(lcs[i - 1][j], lcs[i][j - 1]);
            }
        }
    }

    // 回溯找到匹配的行
    QSet<int> matchedOriginalLines;
    QSet<int> matchedCurrentLines;
    int i = m, j = n;
    while (i > 0 && j > 0)
    {
        if (originalLines[i - 1] == currentLines[j - 1])
        {
            matchedOriginalLines.insert(i - 1);
            matchedCurrentLines.insert(j - 1);
            i--;
            j--;
        }
        else if (lcs[i - 1][j] > lcs[i][j - 1])
        {
            i--;
        }
        else
        {
            j--;
        }
    }

    // 标记未匹配的当前行为新增行
    for (int k = 0; k < currentLines.size(); ++k)
    {
        if (!matchedCurrentLines.contains(k))
        {
            m_newLineNumbers.insert(k + 1);
        }
    }

    // 更新显示
    lineNumberArea->update();
    highlightCurrentLine();
}

// 更新行号区域宽度
void Editor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

// 更新行号区域显示
void Editor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
    {
        lineNumberArea->scroll(0, dy);
    }
    else
    {
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect()))
    {
        updateLineNumberAreaWidth(0);
    }
}

// 处理窗口大小变化事件
void Editor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// 高亮显示当前行及查找匹配结果
void Editor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly())
    {
        // 当前行高亮
        QTextEdit::ExtraSelection selection;
        QColor lineColor = QColor(Qt::yellow).lighter(160);
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    // 查找匹配结果高亮
    for (int i = 0; i < m_matchCursors.size(); ++i)
    {
        const QTextCursor &mc = m_matchCursors[i];
        if (mc.isNull())
            continue;

        QTextEdit::ExtraSelection matchSel;
        QColor matchColor = QColor(Qt::cyan).lighter(180);
        matchSel.format.setBackground(matchColor);
        matchSel.cursor = mc;
        extraSelections.append(matchSel);
    }

    // 当前匹配项用更醒目的颜色标记
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
    {
        QTextCursor cur = m_matchCursors[m_currentMatchIndex];
        if (!cur.isNull())
        {
            QTextEdit::ExtraSelection curSel;
            QColor curColor = QColor(Qt::blue).lighter(170);
            curSel.format.setBackground(curColor);
            curSel.cursor = cur;
            extraSelections.append(curSel);
        }
    }

    // 添加括号高亮（新增部分）
    extraSelections.append(m_bracketSelections);

    setExtraSelections(extraSelections);
}
// 设置原始文本（用于对比新增内容）
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

    // 确保行号区域也更新
    updateLineNumberAreaWidth(0);
}

// 获取当前编辑器字体
QFont Editor::getEditorFont() const
{
    return font();
}

// 从主窗口查找并关联编辑动作
void Editor::findActionsFromMainWindow()
{
    // 尝试获取主窗口
    QMainWindow *mainWindow = nullptr;
    QWidget *currentParent = parentWidget();

    // 向上查找主窗口
    while (currentParent && !mainWindow) {
        mainWindow = qobject_cast<QMainWindow *>(currentParent);
        currentParent = currentParent->parentWidget();  // 这里使用parentWidget()函数
    }

    // 如果没找到主窗口，尝试使用应用程序的主窗口
    if (!mainWindow) {
        foreach (QWidget *widget, QApplication::topLevelWidgets()) {
            mainWindow = qobject_cast<QMainWindow *>(widget);
            if (mainWindow) break;
        }
    }

    if (!mainWindow)
    {
        qDebug() << "警告：未找到主窗口，编辑器动作可能无法正常工作";
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
            undoAction->setText(tr("撤销"));
            undoAction->setToolTip(tr("撤销上一步操作 (Ctrl+Z)"));
        }
        else if (objName == "actionCut")
        {
            cutAction = action;
            cutAction->setText(tr("剪切"));
            cutAction->setToolTip(tr("剪切选中内容到剪贴板 (Ctrl+X)"));
        }
        else if (objName == "actionCopy")
        {
            copyAction = action;
            copyAction->setText(tr("复制"));
            copyAction->setToolTip(tr("复制选中内容到剪贴板 (Ctrl+C)"));
        }
        else if (objName == "actionPaste")
        {
            pasteAction = action;
            pasteAction->setText(tr("粘贴"));
            pasteAction->setToolTip(tr("从剪贴板粘贴内容 (Ctrl+V)"));
        }
        else if (objName == "actionFind")
        {
            findAction = action;
            findAction->setText(tr("查找"));
            findAction->setToolTip(tr("查找文本 (Ctrl+F)"));
        }
        else if (objName == "actionReplace")
        {
            replaceAction = action;
            replaceAction->setText(tr("替换"));
            replaceAction->setToolTip(tr("查找并替换文本 (Ctrl+H)"));
        }
        else if (objName == "actionInsert")
        {
            insertAction = action;
            insertAction->setText(tr("插入"));
            insertAction->setToolTip(tr("插入文本"));
        }
        else if (objName == "actionFont")
        {
            fontAction = action;
            fontAction->setText(tr("文字设置"));
            fontAction->setToolTip(tr("设置编辑器字体 (Ctrl+F12)"));
        }
        else if (objName == "actionHighlightSelection")
        {
            highlightSelectionAction = action;
            highlightSelectionAction->setText(tr("高亮所选"));
            highlightSelectionAction->setToolTip(tr("高亮显示所有选中内容的匹配项"));
        }
        else if (objName == "actionClearHighlights")
        {
            clearHighlightsAction = action;
            clearHighlightsAction->setText(tr("清除高亮"));
            clearHighlightsAction->setToolTip(tr("清除所有高亮显示"));
        }
    }

    // 调试输出动作查找结果
    qDebug() << "动作匹配情况：";
    qDebug() << "撤销动作: " << (undoAction ? "找到" : "未找到");
    qDebug() << "剪切动作: " << (cutAction ? "找到" : "未找到");
    qDebug() << "复制动作: " << (copyAction ? "找到" : "未找到");
    qDebug() << "粘贴动作: " << (pasteAction ? "找到" : "未找到");
    qDebug() << "查找动作: " << (findAction ? "找到" : "未找到");
    qDebug() << "替换动作: " << (replaceAction ? "找到" : "未找到");
    qDebug() << "插入动作: " << (insertAction ? "找到" : "未找到");
    qDebug() << "字体动作: " << (fontAction ? "找到" : "未找到");

    // 设置连接
    setupConnections();
}

// 建立动作与编辑器功能的信号槽连接
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

    // 连接查找动作
    if (findAction)
    {
        disconnect(findAction, 0, 0, 0);
        findAction->setShortcut(QKeySequence::Find); // Ctrl+F
        findAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(findAction, &QAction::triggered, this, &Editor::handleFind);
    }

    // 连接替换动作
    if (replaceAction)
    {
        disconnect(replaceAction, 0, 0, 0);
        replaceAction->setShortcut(QKeySequence::Replace); // Ctrl+H
        replaceAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(replaceAction, &QAction::triggered, this, &Editor::handleReplace);
    }

    // 连接插入动作
    if (insertAction)
    {
        disconnect(insertAction, 0, 0, 0);
        connect(insertAction, &QAction::triggered, this, &Editor::handleInsert);
    }

    // 连接字体设置动作
    if (fontAction)
    {
        disconnect(fontAction, 0, 0, 0);
        fontAction->setShortcut(QKeySequence::fromString("Ctrl+F12"));
        fontAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(fontAction, &QAction::triggered, this, &Editor::handleFontSettings);
    }

    // 连接高亮相关动作
    if (highlightSelectionAction)
    {
        disconnect(highlightSelectionAction, 0, 0, 0);
        connect(highlightSelectionAction, &QAction::triggered, this, &Editor::highlightSelection);
    }

    if (clearHighlightsAction)
    {
        disconnect(clearHighlightsAction, 0, 0, 0);
        connect(clearHighlightsAction, &QAction::triggered, this, &Editor::clearAllHighlights);
    }

    connect(this, &QPlainTextEdit::textChanged, this, &Editor::updateActionStates);

    // 注释快捷键 (Ctrl+/)
    QAction *commentAction = new QAction(tr("注释"), this);
    commentAction->setShortcut(QKeySequence("Ctrl+/"));
    commentAction->setToolTip(tr("注释/取消注释所选行 (Ctrl+/)"));
    connect(commentAction, &QAction::triggered, this, &Editor::handleComment);
    addAction(commentAction);

    // 查找下一个快捷键
    QAction *findNextAction = new QAction(tr("查找下一个"), this);
    findNextAction->setShortcut(QKeySequence::FindNext);
    findNextAction->setToolTip(tr("查找下一个匹配项 (F3)"));
    connect(findNextAction, &QAction::triggered, this, &Editor::findNext);
    addAction(findNextAction);

    // 查找上一个快捷键
    QAction *findPrevAction = new QAction(tr("查找上一个"), this);
    findPrevAction->setShortcut(QKeySequence::FindPrevious);
    findPrevAction->setToolTip(tr("查找上一个匹配项 (Shift+F3)"));
    connect(findPrevAction, &QAction::triggered, this, &Editor::findPrevious);
    addAction(findPrevAction);
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
    {
        cutAction->setEnabled(hasSelection);
    }
    if (copyAction)
    {
        copyAction->setEnabled(hasSelection);
    }

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
    highlightNewLines(); // 撤销后更新新增行标记
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

// 执行粘贴操作（Tab替换逻辑）
void Editor::handlePaste()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();

    // 将粘贴内容中的Tab替换为对应数量的空格
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
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("查找"),
                                               tr("请输入要查找的内容:"), QLineEdit::Normal,
                                               m_searchText, &ok);
    if (!ok || searchText.isEmpty())
        return;

    m_searchText = searchText;
    m_searchFlags = QTextDocument::FindFlags();

    highlightAllMatches();

    // 定位到第一个匹配项
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
    {
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
    }
}

// 处理替换功能
void Editor::handleReplace()
{
    // 获取要查找的文本
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("替换"),
                                               tr("请输入要查找的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || searchText.isEmpty())
        return;

    // 获取替换文本
    QString replaceText = QInputDialog::getText(this, tr("替换"),
                                                tr("请输入替换文本:"), QLineEdit::Normal,
                                                "", &ok);
    if (!ok)
        return;

    // 提供替换选项
    QStringList options;
    options << tr("替换当前匹配项") << tr("替换所有匹配项");
    QString choice = QInputDialog::getItem(this, tr("替换选项"),
                                           tr("请选择操作:"), options, 0, false, &ok);
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
    highlightNewLines(); // 替换后更新新增行标记
}

// 替换当前匹配项
void Editor::replaceCurrent(const QString &searchText, const QString &replaceText)
{
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
    {
        QTextCursor c = m_matchCursors[m_currentMatchIndex];
        c.beginEditBlock();
        c.insertText(replaceText);
        c.endEditBlock();
        highlightAllMatches();
    }
    else
    {
        QTextCursor cursor = document()->find(searchText, textCursor());
        if (!cursor.isNull())
        {
            cursor.insertText(replaceText);
            highlightAllMatches();
        }
        else
        {
            qDebug() << "未找到匹配的文本: " << searchText;
        }
    }
}

// 替换所有匹配项
void Editor::replaceAll(const QString &searchText, const QString &replaceText)
{
    QTextCursor cursor(document());
    int count = 0;
    cursor.beginEditBlock();

    while (!(cursor = document()->find(searchText, cursor)).isNull())
    {
        cursor.insertText(replaceText);
        ++count;
    }

    cursor.endEditBlock();
    qDebug() << "共替换" << count << "处匹配文本";
    highlightAllMatches();
}

// 处理文本插入功能
void Editor::handleInsert()
{
    bool ok;
    QString insertText = QInputDialog::getText(this, tr("插入文本"),
                                               tr("请输入要插入的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || insertText.isEmpty())
        return;

    // 在光标位置插入文本
    QTextCursor cursor = textCursor();
    cursor.insertText(insertText);
    setTextCursor(cursor); // 更新光标位置
}

// 高亮当前选中内容的所有匹配项
void Editor::highlightSelection()
{
    QTextCursor sel = textCursor();
    if (!sel.hasSelection())
        return; // 无选定时直接返回

    QString selectedText = sel.selectedText();
    if (selectedText.isEmpty())
        return;

    // 保存搜索词并使用查找高亮机制
    m_searchText = selectedText;
    m_searchFlags = QTextDocument::FindFlags();
    highlightAllMatches();

    // 定位到第一个匹配项
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
    {
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
        highlightCurrentLine();
    }
}

// 清除所有查找高亮
void Editor::clearAllHighlights()
{
    clearFindHighlights();
}

// 处理字体设置功能
void Editor::handleFontSettings()
{
    bool ok;
    QFont currentFont = getEditorFont(); // 获取当前字体

    // 弹出字体选择对话框
    QFont newFont = QFontDialog::getFont(
        &ok,           // 接收用户是否点击确定
        currentFont,   // 当前字体作为初始值
        this,          // 父窗口
        tr("文字设置") // 对话框标题
    );

    if (ok)
    {
        // 应用新字体
        setEditorFont(newFont);

        // 在状态栏显示字体信息
        QMainWindow *mainWindow = qobject_cast<QMainWindow *>(window());
        if (mainWindow && mainWindow->statusBar())
        {
            mainWindow->statusBar()->showMessage(
                tr("字体已更新: %1 %2点")
                    .arg(newFont.family())
                    .arg(newFont.pointSize()),
                3000 // 3秒后自动消失
            );
        }

        qDebug() << "字体已更新为: " << newFont.family()
                 << ", 大小: " << newFont.pointSize() << "点";
    }
}

// 设置Tab替换为空格的功能
void Editor::setTabReplace(bool replace, int spaces)
{
    if (replace)
    {
        // 设置Tab键插入对应数量的空格
        setTabStopWidth(spaces * fontMetrics().width(' '));
    }
    else
    {
        // 恢复默认Tab行为（8个空格宽度）
        setTabStopWidth(8 * fontMetrics().width(' '));
    }
}

// 处理注释功能（单行注释//）
void Editor::handleComment()
{
    QTextCursor cursor = textCursor();
    bool hasSelection = cursor.hasSelection();

    if (hasSelection)
    {
        // 处理选中区域的注释
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();

        // 选中所有要注释的行
        cursor.setPosition(start);
        cursor.movePosition(QTextCursor::StartOfLine);
        int startLine = cursor.position();

        cursor.setPosition(end);
        cursor.movePosition(QTextCursor::EndOfLine);
        int endLine = cursor.position();

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

        // 替换选中的文本（移除最后一个换行）
        cursor.insertText(processedText.left(processedText.length() - 1));
        setTextCursor(cursor);
    }
    else
    {
        // 注释当前行
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

// 高亮所有匹配项
void Editor::highlightAllMatches()
{
    m_matchCursors.clear();
    if (m_searchText.isEmpty())
    {
        m_currentMatchIndex = -1;
        highlightCurrentLine();
        return;
    }

    QTextCursor cursor(document());
    while (true)
    {
        cursor = document()->find(m_searchText, cursor, m_searchFlags);
        if (cursor.isNull())
            break;
        m_matchCursors.push_back(cursor);
        cursor.setPosition(cursor.position()); // 防止空匹配循环
    }

    m_currentMatchIndex = m_matchCursors.isEmpty() ? -1 : 0;
    highlightCurrentLine();
}

// 查找下一个匹配项
void Editor::findNext()
{
    if (m_searchText.isEmpty())
        return;

    if (m_matchCursors.isEmpty())
    {
        QTextCursor c = document()->find(m_searchText, textCursor(), m_searchFlags);
        if (!c.isNull())
            setTextCursor(c);
        return;
    }

    m_currentMatchIndex = (m_currentMatchIndex + 1) % m_matchCursors.size();
    QTextCursor target = m_matchCursors[m_currentMatchIndex];
    if (!target.isNull())
    {
        setTextCursor(target);
        highlightCurrentLine();
    }
}

// 查找上一个匹配项
void Editor::findPrevious()
{
    if (m_searchText.isEmpty())
        return;

    if (m_matchCursors.isEmpty())
    {
        QTextCursor c = document()->find(m_searchText, textCursor(),
                                         m_searchFlags | QTextDocument::FindBackward);
        if (!c.isNull())
            setTextCursor(c);
        return;
    }

    m_currentMatchIndex = (m_currentMatchIndex - 1 + m_matchCursors.size()) % m_matchCursors.size();
    QTextCursor target = m_matchCursors[m_currentMatchIndex];
    if (!target.isNull())
    {
        setTextCursor(target);
        highlightCurrentLine();
    }
}

// 清除查找高亮
void Editor::clearFindHighlights()
{
    m_searchText.clear();
    m_matchCursors.clear();
    m_currentMatchIndex = -1;
    highlightCurrentLine();
}

// 清除所有高亮
void Editor::clearHighlights()
{
    // 清空查找相关状态
    m_matchCursors.clear();
    m_searchText.clear();
    m_currentMatchIndex = -1;
    highlightAllMatches();

    // 清空手动添加的高亮
    m_selectionExtraSelections.clear();
    setExtraSelections(baseExtraSelections());
}

// 设置高亮相关动作
void Editor::setHighlightActions(QAction *highlightAction, QAction *clearHighlightsAction)
{
    this->highlightSelectionAction = highlightAction;
    this->clearHighlightsAction = clearHighlightsAction;

    if (highlightSelectionAction)
    {
        connect(highlightSelectionAction, &QAction::triggered, this, &Editor::highlightSelection);
    }
    if (clearHighlightsAction)
    {
        connect(clearHighlightsAction, &QAction::triggered, this, &Editor::clearHighlights);
    }
}

// 获取基础的额外选区（当前行+查找结果）
QList<QTextEdit::ExtraSelection> Editor::baseExtraSelections() const
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 当前行高亮
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(Qt::yellow).lighter(190));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    // 查找结果高亮
    QTextCharFormat matchFormat;
    matchFormat.setBackground(Qt::yellow);
    for (const QTextCursor &cursor : m_matchCursors)
    {
        QTextEdit::ExtraSelection matchSelection;
        matchSelection.format = matchFormat;
        matchSelection.cursor = cursor;
        extraSelections.append(matchSelection);
    }

    // 叠加手动选中高亮
    extraSelections.append(m_selectionExtraSelections);

    return extraSelections;
}

// 高亮匹配括号
void Editor::highlightMatchingBracket()
{
    // 清除之前的括号高亮

    QTextCursor cursor = textCursor();
    int position = cursor.position();
    QTextDocument *doc = document();

    // 获取当前字符和前一个字符
    QChar currentChar, prevChar;
    if (position > 0) {
        prevChar = doc->characterAt(position - 1);
    }
    if (position < doc->characterCount()) {
        currentChar = doc->characterAt(position);
    }

    // 检查是否是左括号或右括号
    if (m_matchingPairs.contains(prevChar)) {
        // 处理左括号
        QChar matchChar = m_matchingPairs[prevChar];
        int matchPos = findMatchingBracket(position - 1, prevChar, matchChar, 1);
        if (matchPos != -1) {
            highlightBracketPair(position - 1, matchPos);
        }
    } else if (m_matchingPairs.values().contains(currentChar)) {
        // 处理右括号，找到对应的左括号
        QChar matchChar;
        for (auto it = m_matchingPairs.begin(); it != m_matchingPairs.end(); ++it) {
            if (it.value() == currentChar) {
                matchChar = it.key();
                break;
            }
        }

        int matchPos = findMatchingBracket(position, currentChar, matchChar, -1);
        if (matchPos != -1) {
            highlightBracketPair(matchPos, position);
        }
    }
}

// 查找匹配的括号
// 3. 增强括号匹配查找逻辑
int Editor::findMatchingBracket(int startPos, QChar bracket, QChar matchBracket, int direction)
{
    QTextDocument *doc = document();
    int depth = 1;
    int currentPos = startPos + direction;

    // 特别处理小括号的查找逻辑
    while (currentPos >= 0 && currentPos < doc->characterCount()) {
        QChar c = doc->characterAt(currentPos);

        // 忽略字符串中的括号
        static QSet<QChar> quotes = {'"', '\''};
        static bool inString = false;
        static QChar currentQuote;

        if (quotes.contains(c)) {
            if (!inString) {
                inString = true;
                currentQuote = c;
            } else if (c == currentQuote) {
                inString = false;
            }
            currentPos += direction;
            continue;
        }

        // 如果在字符串中，跳过所有字符
        if (inString) {
            currentPos += direction;
            continue;
        }

        // 括号匹配逻辑
        if (c == bracket) {
            depth++;  // 遇到相同括号，深度增加
        } else if (c == matchBracket) {
            depth--;  // 遇到匹配括号，深度减少
            if (depth == 0) {
                return currentPos;  // 找到匹配的括号
            }
        }

        currentPos += direction;
    }

    return -1;  // 未找到匹配的括号
}

// 高亮显示括号对
void Editor::highlightBracketPair(int pos1, int pos2)
{
    // 创建高亮格式
    QTextCharFormat format;
    format.setBackground(QColor(255, 255, 153));  // 浅黄色背景
    format.setForeground(QColor(255, 0, 0));     // 红色文字
    format.setFontWeight(QFont::Bold);

    // 第一个括号
    QTextEdit::ExtraSelection selection1;
    selection1.format = format;
    selection1.cursor = textCursor();
    selection1.cursor.setPosition(pos1);
    selection1.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

    // 第二个括号
    QTextEdit::ExtraSelection selection2;
    selection2.format = format;
    selection2.cursor = textCursor();
    selection2.cursor.setPosition(pos2);
    selection2.cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);

    // 保存并应用高亮
    m_bracketSelections.clear();
    m_bracketSelections.append(selection1);
    m_bracketSelections.append(selection2);

    // 更新高亮显示
    updateBracketHighlight();
}

// 清除括号高亮
void Editor::clearBracketHighlight()
{
    if (!m_bracketSelections.isEmpty()) {
        m_bracketSelections.clear();
        updateBracketHighlight();
    }
}

void Editor::updateBracketHighlight()
{
    // 获取现有的高亮选区
    QList<QTextEdit::ExtraSelection> selections = extraSelections();

    // 移除之前的括号高亮
    for (int i = selections.size() - 1; i >= 0; --i) {
        // 通过背景色识别括号高亮
        if (selections[i].format.background().color() == QColor(255, 255, 153)) {
            selections.removeAt(i);
        }
    }

    // 添加新的括号高亮
    selections.append(m_bracketSelections);

    // 应用更新后的高亮
    setExtraSelections(selections);
}
// 更新括号高亮显示


void Editor::wheelEvent(QWheelEvent *event)
{
    // 检查是否按住Ctrl键
    if (event->modifiers() & Qt::ControlModifier) {
        // 获取当前字体
        QFont currentFont = font();
        int fontSize = currentFont.pointSize();

        // 根据滚轮方向调整字体大小
        if (event->angleDelta().y() > 0) {
            // 滚轮向上，增大字体
            fontSize += 1;
        } else {
            // 滚轮向下，减小字体
            fontSize -= 1;
        }

        // 限制字体大小范围（例如 6-72）
        fontSize = qMax(6, qMin(72, fontSize));

        // 设置新字体
        currentFont.setPointSize(fontSize);
        setFont(currentFont);

        // 更新Tab宽度
        setTabReplace(true, 4);

        // 接受事件，阻止继续传递
        event->accept();
    } else {
        // 非Ctrl键时执行默认滚轮行为（滚动文本）
        QPlainTextEdit::wheelEvent(event);
    }
}
EditorSyntaxHighlighter::EditorSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 关键字格式
    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);

    // C++关键字
    QStringList keywordPatterns = {
        "\\bchar\\b", "\\bclass\\b", "\\bconst\\b",
        "\\bdouble\\b", "\\benum\\b", "\\bexplicit\\b",
        "\\bfriend\\b", "\\binline\\b", "\\bint\\b",
        "\\blong\\b", "\\bnamespace\\b", "\\boperator\\b",
        "\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b",
        "\\bshort\\b", "\\bsignals\\b", "\\bsigned\\b",
        "\\bslots\\b", "\\bstatic\\b", "\\bstruct\\b",
        "\\btemplate\\b", "\\btypedef\\b", "\\btypename\\b",
        "\\bunion\\b", "\\bunsigned\\b", "\\bvirtual\\b",
        "\\bvoid\\b", "\\bvolatile\\b", "\\bbool\\b",
        "\\bif\\b", "\\belse\\b", "\\bswitch\\b", "\\bcase\\b",
        "\\bdefault\\b", "\\bfor\\b", "\\bwhile\\b", "\\bdo\\b",
        "\\breturn\\b", "\\bbreak\\b", "\\bcontinue\\b", "\\bdelete\\b",
        "\\bnew\\b", "\\bthis\\b", "\\bsizeof\\b", "\\btrue\\b", "\\bfalse\\b"
    };

    // 添加关键字规则
    for (const QString &pattern : keywordPatterns) {
        highlightingRules.append({QRegularExpression(pattern), keywordFormat});
    }

    // 类名格式
    classFormat.setForeground(Qt::darkMagenta);
    classFormat.setFontWeight(QFont::Bold);
    highlightingRules.append({QRegularExpression("\\bQ[A-Za-z]+\\b"), classFormat});

    // 函数格式
    functionFormat.setForeground(Qt::darkCyan);
    highlightingRules.append({QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()"), functionFormat});

    // 引号字符串格式
    quotationFormat.setForeground(Qt::darkGreen);
    highlightingRules.append({QRegularExpression("\".*\""), quotationFormat});
    highlightingRules.append({QRegularExpression("'.*'"), quotationFormat});

    // 数字格式
    numberFormat.setForeground(Qt::darkRed);
    highlightingRules.append({QRegularExpression("\\b[0-9]+\\b"), numberFormat});
    highlightingRules.append({QRegularExpression("\\b0x[0-9A-Fa-f]+\\b"), numberFormat});
    highlightingRules.append({QRegularExpression("\\b[0-9]+\\.[0-9]+\\b"), numberFormat});

    // 单行注释格式
    singleLineCommentFormat.setForeground(Qt::gray);
    highlightingRules.append({QRegularExpression("//[^\n]*"), singleLineCommentFormat});

    // 多行注释格式
    multiLineCommentFormat.setForeground(Qt::gray);
    commentStartExpression = QRegularExpression("/\\*");
    commentEndExpression = QRegularExpression("\\*/");
}

void EditorSyntaxHighlighter::highlightBlock(const QString &text)
{
    // 应用所有单行文法规则
    for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // 处理多行注释
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(commentStartExpression);

    while (startIndex >= 0) {
        QRegularExpressionMatch match = commentEndExpression.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;

        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + match.capturedLength();
        }

        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
    }
}
Editor::~Editor()
{
    if (highlighter) {
        delete highlighter; // highlighter可能未被初始化或已由Qt管理
        highlighter = nullptr;
    }
    // 注释掉的删除lineNumberArea和动作对象的代码
}
