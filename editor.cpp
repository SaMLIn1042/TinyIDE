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

    // 初始化成对符号映射（新增）
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

    // 初始更新行号区域宽度
    updateLineNumberAreaWidth(0);
    // 高亮当前行
    highlightCurrentLine();
    // 初始检查新增行
    highlightNewLines();
}
// 新增：处理文本输入，实现自动补全成对符号
void Editor::keyPressEvent(QKeyEvent *event)
{
    // 1. 处理特殊按键（如退格键）
    if (event->key() == Qt::Key_Backspace) {
        // 获取当前光标
        QTextCursor cursor = textCursor();

        // 检查是否需要删除成对符号
        if (cursor.hasSelection()) {
            // 有选定时，直接调用父类处理
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        // 保存当前位置
        int pos = cursor.position();
        // 移动到前一个字符
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
        QString leftChar = cursor.selectedText();

        // 移动到后一个字符
        cursor.setPosition(pos);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        QString rightChar = cursor.selectedText();

        // 如果是成对符号，同时删除两个
        if (m_matchingPairs.contains(leftChar[0]) &&
            m_matchingPairs[leftChar[0]] == rightChar[0]) {
            // 先删除左边字符
            cursor.setPosition(pos - 1);
            cursor.deleteChar();
            // 再删除右边字符
            cursor.deleteChar();
            event->accept();
            return;
        }
    }

    // 2. 处理普通字符输入（成对符号补充）
    QString inputText = event->text();
    if (!inputText.isEmpty()) {  // 确保有输入字符
        QChar inputChar = inputText.at(0);

        // 检查是否是需要自动补全的左符号
        if (m_matchingPairs.contains(inputChar)) {
            // 获取匹配的右符号
            QChar matchingChar = m_matchingPairs[inputChar];

            // 保存当前光标位置
            QTextCursor cursor = textCursor();
            int originalPos = cursor.position();

            // 插入左符号
            cursor.insertText(inputChar);

            // 插入右符号
            cursor.insertText(matchingChar);

            // 将光标移回两个符号之间
            cursor.setPosition(originalPos + 1);
            setTextCursor(cursor);

            // 标记事件已处理
            event->accept();
            return;
        }
    }

    // 3. 其他情况：调用父类方法处理
    QPlainTextEdit::keyPressEvent(event);
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
                painter.setPen(Qt::red);
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

    // 使用最长公共子序列(LCS)算法来确定哪些行是新增的
    // 创建二维数组存储LCS长度
    int m = originalLines.size();
    int n = currentLines.size();
    QVector<QVector<int>> lcs(m + 1, QVector<int>(n + 1, 0));

    // 计算LCS
    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            if (originalLines[i-1] == currentLines[j-1]) {
                lcs[i][j] = lcs[i-1][j-1] + 1;
            } else {
                lcs[i][j] = qMax(lcs[i-1][j], lcs[i][j-1]);
            }
        }
    }

    // 回溯找到匹配的行
    QSet<int> matchedOriginalLines;
    QSet<int> matchedCurrentLines;
    int i = m, j = n;
    while (i > 0 && j > 0) {
        if (originalLines[i-1] == currentLines[j-1]) {
            matchedOriginalLines.insert(i-1);
            matchedCurrentLines.insert(j-1);
            i--;
            j--;
        } else if (lcs[i-1][j] > lcs[i][j-1]) {
            i--;
        } else {
            j--;
        }
    }

    // 标记未匹配的当前行（新增行）
    for (int k = 0; k < currentLines.size(); ++k) {
        if (!matchedCurrentLines.contains(k)) {
            m_newLineNumbers.insert(k + 1); // 行号从1开始
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

    }

    // --- 查找匹配高亮（新增） ---
    // 所有匹配使用较浅的颜色
    for (int i = 0; i < m_matchCursors.size(); ++i)
    {
        const QTextCursor &mc = m_matchCursors[i];
        if (mc.isNull()) continue;
        QTextEdit::ExtraSelection matchSel;
        QColor matchColor = QColor(Qt::cyan).lighter(180);
        matchSel.format.setBackground(matchColor);
        matchSel.cursor = mc; // 保持选区
        extraSelections.append(matchSel);
    }

    // 当前匹配用更醒目的颜色覆盖
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

            // 将视图滚动到当前匹配
            // 将视图滚动到当前匹配并尽量居中显示（替换 ensureVisible(...)）
            QRect r = cursorRect(cur);
            QScrollBar *hbar = horizontalScrollBar();
            QScrollBar *vbar = verticalScrollBar();

            int centerX = r.center().x();
            int centerY = r.center().y();

            // 计算新的滚动位置（以使匹配项位于视口中心）
            int newH = hbar->value() + centerX - viewport()->width() / 2;
            int newV = vbar->value() + centerY - viewport()->height() / 2;

            // 限制在合理范围内
            newH = qBound(hbar->minimum(), newH, hbar->maximum());
            newV = qBound(vbar->minimum(), newV, vbar->maximum());

            hbar->setValue(newH);
            vbar->setValue(newV);

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
        else if (objName == "actionHighlightSelection")
        { // 新增：高亮相关
            highlightSelectionAction = action;
        }
        else if (objName == "actionClearHighlights")
        { // 新增：高亮相关
            clearHighlightsAction = action;
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

    //新增：高亮
    // 绑定“高亮所选”
    if (highlightSelectionAction) {
        disconnect(highlightSelectionAction, 0, 0, 0);
        connect(highlightSelectionAction, &QAction::triggered, this, &Editor::highlightSelection);
    }

    // 绑定“清除高亮”
    if (clearHighlightsAction) {
        disconnect(clearHighlightsAction, 0, 0, 0);
        connect(clearHighlightsAction, &QAction::triggered, this, &Editor::clearAllHighlights);
    }

    connect(this, &QPlainTextEdit::textChanged, this, &Editor::updateActionStates);
    QAction *commentAction = new QAction(this);
    commentAction->setShortcut(QKeySequence("Ctrl+/"));
    connect(commentAction, &QAction::triggered, this, &Editor::handleComment);
    addAction(commentAction);

    QAction *findNextAction = new QAction(this);
    findNextAction->setShortcut(QKeySequence::FindNext);
    connect(findNextAction, &QAction::triggered, this, &Editor::findNext);
    addAction(findNextAction);

    QAction *findPrevAction = new QAction(this);
    findPrevAction->setShortcut(QKeySequence::FindPrevious);
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
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("查找"),
                                               tr("输入要查找的内容:"), QLineEdit::Normal,
                                               m_searchText, &ok);
    if (!ok || searchText.isEmpty()) return;

    m_searchText = searchText;
    m_searchFlags = QTextDocument::FindFlags();
    // 如果需要大小写敏感： m_searchFlags |= QTextDocument::FindCaseSensitively;

    highlightAllMatches();

    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
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


void Editor::replaceCurrent(const QString &searchText, const QString &replaceText)
{
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size()) {
        QTextCursor c = m_matchCursors[m_currentMatchIndex];
        c.beginEditBlock();
        c.insertText(replaceText);
        c.endEditBlock();
        highlightAllMatches();
    } else {
        QTextCursor cursor = document()->find(searchText, textCursor());
        if (!cursor.isNull()) { cursor.insertText(replaceText); highlightAllMatches(); }
        else qDebug() << "未找到匹配的文本: " << searchText;
    }
}

void Editor::replaceAll(const QString &searchText, const QString &replaceText)
{
    QTextCursor cursor(document());
    int count = 0;
    cursor.beginEditBlock();
    while (!(cursor = document()->find(searchText, cursor)).isNull()) {
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

// 高亮当前选中内容的所有匹配项（QAction 触发）
void Editor::highlightSelection()
{
    QTextCursor sel = textCursor();
    if (!sel.hasSelection()) {
        // 如果没有选中，直接不做任何事或提示（这里静默返回）
        return;
    }

    QString selectedText = sel.selectedText();
    if (selectedText.isEmpty()) return;

    // 保存搜索词并使用与查找相同的高亮机制
    m_searchText = selectedText;
    m_searchFlags = QTextDocument::FindFlags();
    // 需要匹配整个单词可加入额外逻辑；目前为简单文本匹配

    highlightAllMatches();

    // 跳到第一个匹配（若存在）
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size()) {
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
        // 确保可见（我们已有 highlightCurrentLine 内的滚动逻辑）
        highlightCurrentLine();
    }
}

// 清除全局查找高亮（QAction 触发）
void Editor::clearAllHighlights()
{
    // 直接调用已有函数或做相同动作
    clearFindHighlights(); // 我们之前实现过这个函数，会清空 m_searchText / m_matchCursors
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

void Editor::highlightAllMatches()
{
    m_matchCursors.clear();
    if (m_searchText.isEmpty()) { m_currentMatchIndex = -1; highlightCurrentLine(); return; }

    QTextCursor cursor(document());
    while (true) {
        cursor = document()->find(m_searchText, cursor, m_searchFlags);
        if (cursor.isNull()) break;
        m_matchCursors.push_back(cursor);
        // 防止空匹配造成循环
        cursor.setPosition(cursor.position());
    }

    if (!m_matchCursors.isEmpty()) m_currentMatchIndex = 0;
    else m_currentMatchIndex = -1;

    highlightCurrentLine();
}

void Editor::findNext()
{
    if (m_searchText.isEmpty()) return;
    if (m_matchCursors.isEmpty()) {
        QTextCursor c = document()->find(m_searchText, textCursor(), m_searchFlags);
        if (!c.isNull()) setTextCursor(c);
        return;
    }
    m_currentMatchIndex = (m_currentMatchIndex + 1) % m_matchCursors.size();
    QTextCursor target = m_matchCursors[m_currentMatchIndex];
    if (!target.isNull()) { setTextCursor(target); highlightCurrentLine(); }
}

void Editor::findPrevious()
{
    if (m_searchText.isEmpty()) return;
    if (m_matchCursors.isEmpty()) {
        QTextCursor c = document()->find(m_searchText, textCursor(), m_searchFlags | QTextDocument::FindBackward);
        if (!c.isNull()) setTextCursor(c);
        return;
    }
    m_currentMatchIndex = (m_currentMatchIndex - 1 + m_matchCursors.size()) % m_matchCursors.size();
    QTextCursor target = m_matchCursors[m_currentMatchIndex];
    if (!target.isNull()) { setTextCursor(target); highlightCurrentLine(); }
}

void Editor::clearFindHighlights()
{
    m_searchText.clear();
    m_matchCursors.clear();
    m_currentMatchIndex = -1;
    highlightCurrentLine();
}

void Editor::clearHighlights()
{
    // 清空查找相关的状态
    m_matchCursors.clear();
    m_searchText.clear();
    m_currentMatchIndex = -1;
    highlightAllMatches();  // 刷新查找高亮

    // 清空手动添加的高亮
    m_selectionExtraSelections.clear();
    setExtraSelections(baseExtraSelections());
}


void Editor::setHighlightActions(QAction *highlightAction, QAction *clearHighlightsAction)
{
    this->highlightSelectionAction = highlightAction;
    this->clearHighlightsAction = clearHighlightsAction;

    if (highlightSelectionAction) {
        connect(highlightSelectionAction, &QAction::triggered, this, &Editor::highlightSelection);
    }
    if (clearHighlightsAction) {
        connect(clearHighlightsAction, &QAction::triggered, this, &Editor::clearHighlights);
    }
}

QList<QTextEdit::ExtraSelection> Editor::baseExtraSelections() const
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 当前行高亮
    if (!isReadOnly()) {
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
    for (const QTextCursor &cursor : m_matchCursors) {
        QTextEdit::ExtraSelection matchSelection;
        matchSelection.format = matchFormat;
        matchSelection.cursor = cursor;
        extraSelections.append(matchSelection);
    }

    // 叠加手动选中高亮
    extraSelections.append(m_selectionExtraSelections);

    return extraSelections;
}

