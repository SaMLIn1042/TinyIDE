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

/**
 * @brief 初始化文本编辑器及相关组件
 * @param parent 父窗口指针
 */
Editor::Editor(QWidget *parent) : QPlainTextEdit(parent),
                                  undoAction(nullptr), cutAction(nullptr),
                                  copyAction(nullptr), pasteAction(nullptr),
                                  findAction(nullptr), replaceAction(nullptr),
                                  insertAction(nullptr), fontAction(nullptr),
                                  lineNumberArea(new LineNumberArea(this))
{
    // 加载Qt中文翻译支持
    loadChineseTranslation();

    setUndoRedoEnabled(true);    // 启用撤销/重做功能
    setTabReplace(true, 4);      // 初始化Tab替换为4个空格
    findActionsFromMainWindow(); // 从主窗口查找关联动作
    setupConnections();          // 建立信号槽连接
    updateActionStates();        // 更新动作状态

    // 初始化原始文本（用于跟踪新增内容）
    m_originalText = toPlainText();

    // 初始化成对符号映射（用于自动补全）
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

/**
 * @brief 加载Qt中文翻译文件，使标准对话框显示中文
 */
void Editor::loadChineseTranslation()
{
    // 为Qt标准组件加载中文翻译
    QTranslator *qtTranslator = new QTranslator(qApp);
    if (qtTranslator->load("qt_zh_CN.qm", QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        qApp->installTranslator(qtTranslator);
    }

    QTranslator *qtBaseTranslator = new QTranslator(qApp);
    if (qtBaseTranslator->load("qtbase_zh_CN.qm", QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        qApp->installTranslator(qtBaseTranslator);
    }
}

/**
 * @brief 处理文本输入事件，实现成对符号自动补全
 * @param event 按键事件对象
 */
void Editor::keyPressEvent(QKeyEvent *event)
{
    // 1. 处理退格键（删除成对符号）
    if (event->key() == Qt::Key_Backspace) {
        QTextCursor cursor = textCursor();

        // 有选定时，直接调用父类处理
        if (cursor.hasSelection()) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        // 保存当前位置
        int pos = cursor.position();
        // 获取左侧字符
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
        QString leftChar = cursor.selectedText();

        // 获取右侧字符
        cursor.setPosition(pos);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        QString rightChar = cursor.selectedText();

        // 如果是成对符号，同时删除两个
        if (m_matchingPairs.contains(leftChar[0]) &&
            m_matchingPairs[leftChar[0]] == rightChar[0]) {
            cursor.setPosition(pos - 1);
            cursor.deleteChar(); // 删除左侧字符
            cursor.deleteChar(); // 删除右侧字符
            event->accept();
            return;
        }
    }

    // 2. 处理普通字符输入（自动补全成对符号）
    QString inputText = event->text();
    if (!inputText.isEmpty()) {
        QChar inputChar = inputText.at(0);

        // 检查是否是需要自动补全的左符号
        if (m_matchingPairs.contains(inputChar)) {
            QChar matchingChar = m_matchingPairs[inputChar]; // 获取匹配的右符号

            // 保存当前光标位置
            QTextCursor cursor = textCursor();
            int originalPos = cursor.position();

            // 插入左符号和右符号
            cursor.insertText(inputChar);
            cursor.insertText(matchingChar);

            // 将光标移回两个符号之间
            cursor.setPosition(originalPos + 1);
            setTextCursor(cursor);

            event->accept();
            return;
        }
    }

    // 3. 其他情况：调用父类方法处理
    QPlainTextEdit::keyPressEvent(event);
}

/**
 * @brief 计算行号区域所需宽度
 * @return 行号区域宽度
 */
int Editor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());

    // 计算最大行号所需的位数
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    // 计算宽度（包含边距）
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

/**
 * @brief 绘制行号区域
 * @param event 绘图事件对象
 */
void Editor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), Qt::lightGray); // 填充背景

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    // 绘制可见行的行号
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);

            // 新增行用红色显示行号
            if (m_newLineNumbers.contains(blockNumber + 1)) {
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

/**
 * @brief 文本变化时触发，更新新增行标记
 */
void Editor::onTextChanged()
{
    highlightNewLines();
}

/**
 * @brief 高亮显示新增行（与原始文本对比）
 */
void Editor::highlightNewLines()
{
    m_newLineNumbers.clear();

    QString currentText = toPlainText();
    QStringList originalLines = m_originalText.split('\n');
    QStringList currentLines = currentText.split('\n');

    // 使用最长公共子序列(LCS)算法确定新增行
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

    // 标记未匹配的当前行为新增行（行号从1开始）
    for (int k = 0; k < currentLines.size(); ++k) {
        if (!matchedCurrentLines.contains(k)) {
            m_newLineNumbers.insert(k + 1);
        }
    }

    // 更新显示
    lineNumberArea->update();
    highlightCurrentLine();
}

/**
 * @brief 更新行号区域宽度
 * @param newBlockCount 新的块数量（未使用）
 */
void Editor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

/**
 * @brief 更新行号区域显示
 * @param rect 需要更新的区域
 * @param dy 垂直滚动距离
 */
void Editor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy) {
        lineNumberArea->scroll(0, dy);
    } else {
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

/**
 * @brief 处理窗口大小变化事件
 * @param event 大小变化事件对象
 */
void Editor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

/**
 * @brief 高亮显示当前行及查找匹配结果
 */
void Editor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
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
    // 所有匹配项用浅色标记
    for (int i = 0; i < m_matchCursors.size(); ++i) {
        const QTextCursor &mc = m_matchCursors[i];
        if (mc.isNull()) continue;

        QTextEdit::ExtraSelection matchSel;
        QColor matchColor = QColor(Qt::cyan).lighter(180);
        matchSel.format.setBackground(matchColor);
        matchSel.cursor = mc;
        extraSelections.append(matchSel);
    }

    // 当前匹配项用更醒目的颜色标记
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size()) {
        QTextCursor cur = m_matchCursors[m_currentMatchIndex];
        if (!cur.isNull()) {
            QTextEdit::ExtraSelection curSel;
            QColor curColor = QColor(Qt::blue).lighter(170);
            curSel.format.setBackground(curColor);
            curSel.cursor = cur;
            extraSelections.append(curSel);

            // 将当前匹配项滚动到视图中心
            QRect r = cursorRect(cur);
            QScrollBar *hbar = horizontalScrollBar();
            QScrollBar *vbar = verticalScrollBar();

            int centerX = r.center().x();
            int centerY = r.center().y();

            // 计算新的滚动位置
            int newH = hbar->value() + centerX - viewport()->width() / 2;
            int newV = vbar->value() + centerY - viewport()->height() / 2;

            // 限制在有效范围内
            newH = qBound(hbar->minimum(), newH, hbar->maximum());
            newV = qBound(vbar->minimum(), newV, vbar->maximum());

            hbar->setValue(newH);
            vbar->setValue(newV);
        }
    }

    setExtraSelections(extraSelections);
}

/**
 * @brief 设置原始文本（用于对比新增内容）
 * @param text 原始文本内容
 */
void Editor::setOriginalText(const QString &text)
{
    m_originalText = text;
    highlightNewLines(); // 重新计算新增行
}

/**
 * @brief 获取编辑器中的纯文本内容
 * @return 编辑器文本
 */
QString Editor::getCodeText() const
{
    return toPlainText();
}

/**
 * @brief 设置编辑器字体
 * @param font 新字体
 */
void Editor::setEditorFont(const QFont &font)
{
    setFont(font);
    // 更新Tab宽度以适应新字体
    setTabReplace(true, 4);
}

/**
 * @brief 获取当前编辑器字体
 * @return 当前字体
 */
QFont Editor::getEditorFont() const
{
    return font();
}

/**
 * @brief 从主窗口查找并关联编辑动作
 */
void Editor::findActionsFromMainWindow()
{
    // 获取父窗口中的动作对象
    QMainWindow *mainWindow = qobject_cast<QMainWindow *>(window());
    if (!mainWindow) {
        qDebug() << "错误：未找到主窗口";
        return;
    }

    // 遍历所有动作并匹配对象名
    QList<QAction *> allActions = mainWindow->findChildren<QAction *>();
    foreach (QAction *action, allActions) {
        const QString &objName = action->objectName();

        // 根据对象名关联对应动作
        if (objName == "actionUndo") {
            undoAction = action;
            undoAction->setText(tr("撤销"));
            undoAction->setToolTip(tr("撤销上一步操作 (Ctrl+Z)"));
        } else if (objName == "actionCut") {
            cutAction = action;
            cutAction->setText(tr("剪切"));
            cutAction->setToolTip(tr("剪切选中内容到剪贴板 (Ctrl+X)"));
        } else if (objName == "actionCopy") {
            copyAction = action;
            copyAction->setText(tr("复制"));
            copyAction->setToolTip(tr("复制选中内容到剪贴板 (Ctrl+C)"));
        } else if (objName == "actionPaste") {
            pasteAction = action;
            pasteAction->setText(tr("粘贴"));
            pasteAction->setToolTip(tr("从剪贴板粘贴内容 (Ctrl+V)"));
        } else if (objName == "actionFind") {
            findAction = action;
            findAction->setText(tr("查找"));
            findAction->setToolTip(tr("查找文本 (Ctrl+F)"));
        } else if (objName == "actionReplace") {
            replaceAction = action;
            replaceAction->setText(tr("替换"));
            replaceAction->setToolTip(tr("查找并替换文本 (Ctrl+H)"));
        } else if (objName == "actionInsert") {
            insertAction = action;
            insertAction->setText(tr("插入"));
            insertAction->setToolTip(tr("插入文本"));
        } else if (objName == "actionFont") {
            fontAction = action;
            fontAction->setText(tr("文字设置"));
            fontAction->setToolTip(tr("设置编辑器字体 (Ctrl+F12)"));
        } else if (objName == "actionHighlightSelection") {
            highlightSelectionAction = action;
            highlightSelectionAction->setText(tr("高亮所选"));
            highlightSelectionAction->setToolTip(tr("高亮显示所有选中内容的匹配项"));
        } else if (objName == "actionClearHighlights") {
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
}

/**
 * @brief 建立动作与编辑器功能的信号槽连接
 */
void Editor::setupConnections()
{
    // 连接撤销动作
    if (undoAction) {
        disconnect(undoAction, 0, 0, 0);
        connect(undoAction, &QAction::triggered, this, &Editor::handleUndo);
        connect(this, &QPlainTextEdit::undoAvailable, undoAction, &QAction::setEnabled);
    }

    // 连接剪切动作
    if (cutAction) {
        disconnect(cutAction, 0, 0, 0);
        connect(cutAction, &QAction::triggered, this, &Editor::handleCut);
        connect(this, &QPlainTextEdit::copyAvailable, cutAction, &QAction::setEnabled);
    }

    // 连接复制动作
    if (copyAction) {
        disconnect(copyAction, 0, 0, 0);
        connect(copyAction, &QAction::triggered, this, &Editor::handleCopy);
        connect(this, &QPlainTextEdit::copyAvailable, copyAction, &QAction::setEnabled);
    }

    // 连接粘贴动作
    if (pasteAction) {
        disconnect(pasteAction, 0, 0, 0);
        connect(pasteAction, &QAction::triggered, this, &Editor::handlePaste);
        connect(QApplication::clipboard(), &QClipboard::dataChanged,
                this, &Editor::updatePasteState);
    }

    // 连接查找动作（设置标准快捷键）
    if (findAction) {
        disconnect(findAction, 0, 0, 0);
        findAction->setShortcut(QKeySequence::Find); // Ctrl+F
        findAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(findAction, &QAction::triggered, this, &Editor::handleFind);
    }

    // 连接替换动作（设置标准快捷键）
    if (replaceAction) {
        disconnect(replaceAction, 0, 0, 0);
        replaceAction->setShortcut(QKeySequence::Replace); // Ctrl+H
        replaceAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(replaceAction, &QAction::triggered, this, &Editor::handleReplace);
    }

    // 连接插入动作
    if (insertAction) {
        disconnect(insertAction, 0, 0, 0);
        connect(insertAction, &QAction::triggered, this, &Editor::handleInsert);
    }

    // 连接字体设置动作
    if (fontAction) {
        disconnect(fontAction, 0, 0, 0);
        fontAction->setShortcut(QKeySequence::fromString("Ctrl+F12"));
        fontAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(fontAction, &QAction::triggered, this, &Editor::handleFontSettings);
    }

    // 连接高亮相关动作
    if (highlightSelectionAction) {
        disconnect(highlightSelectionAction, 0, 0, 0);
        connect(highlightSelectionAction, &QAction::triggered, this, &Editor::highlightSelection);
    }

    if (clearHighlightsAction) {
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

/**
 * @brief 更新编辑动作的启用状态
 */
void Editor::updateActionStates()
{
    // 更新撤销动作状态
    if (undoAction) {
        undoAction->setEnabled(document()->isUndoAvailable());
    }

    // 根据文本选择状态更新剪切/复制动作
    bool hasSelection = textCursor().hasSelection();
    if (cutAction) {
        cutAction->setEnabled(hasSelection);
    }
    if (copyAction) {
        copyAction->setEnabled(hasSelection);
    }

    // 更新粘贴动作状态
    updatePasteState();
}

/**
 * @brief 更新粘贴动作的启用状态（基于剪贴板内容）
 */
void Editor::updatePasteState()
{
    if (pasteAction) {
        pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
    }
}

/**
 * @brief 执行撤销操作
 */
void Editor::handleUndo()
{
    undo();
    updateActionStates();
    highlightNewLines(); // 撤销后更新新增行标记
}

/**
 * @brief 执行剪切操作
 */
void Editor::handleCut()
{
    cut();
    updateActionStates();
}

/**
 * @brief 执行复制操作
 */
void Editor::handleCopy()
{
    copy();
    updateActionStates();
}

/**
 * @brief 执行粘贴操作（补充Tab替换逻辑）
 */
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

/**
 * @brief 处理查找功能
 */
void Editor::handleFind()
{
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("查找"),
                                               tr("请输入要查找的内容:"), QLineEdit::Normal,
                                               m_searchText, &ok);
    if (!ok || searchText.isEmpty()) return;

    m_searchText = searchText;
    m_searchFlags = QTextDocument::FindFlags();
    // 如需大小写敏感，可添加：m_searchFlags |= QTextDocument::FindCaseSensitively;

    highlightAllMatches();

    // 定位到第一个匹配项
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size()) {
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
    }
}

/**
 * @brief 处理替换功能
 */
void Editor::handleReplace()
{
    // 获取要查找的文本
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("替换"),
                                               tr("请输入要查找的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || searchText.isEmpty()) return;

    // 获取替换文本
    QString replaceText = QInputDialog::getText(this, tr("替换"),
                                                tr("请输入替换文本:"), QLineEdit::Normal,
                                                "", &ok);
    if (!ok) return;

    // 提供替换选项
    QStringList options;
    options << tr("替换当前匹配项") << tr("替换所有匹配项");
    QString choice = QInputDialog::getItem(this, tr("替换选项"),
                                           tr("请选择操作:"), options, 0, false, &ok);
    if (!ok) return;

    // 执行选择的替换操作
    if (choice == options[0]) {
        replaceCurrent(searchText, replaceText); // 替换当前匹配项
    } else {
        replaceAll(searchText, replaceText); // 替换所有匹配项
    }
    highlightNewLines(); // 替换后更新新增行标记
}

/**
 * @brief 替换当前匹配项
 * @param searchText 查找文本
 * @param replaceText 替换文本
 */
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
        if (!cursor.isNull()) {
            cursor.insertText(replaceText);
            highlightAllMatches();
        } else {
            qDebug() << "未找到匹配的文本: " << searchText;
        }
    }
}

/**
 * @brief 替换所有匹配项
 * @param searchText 查找文本
 * @param replaceText 替换文本
 */
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

/**
 * @brief 处理文本插入功能
 */
void Editor::handleInsert()
{
    bool ok;
    QString insertText = QInputDialog::getText(this, tr("插入文本"),
                                               tr("请输入要插入的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || insertText.isEmpty()) return;

    // 在光标位置插入文本
    QTextCursor cursor = textCursor();
    cursor.insertText(insertText);
    setTextCursor(cursor); // 更新光标位置
}

/**
 * @brief 高亮当前选中内容的所有匹配项
 */
void Editor::highlightSelection()
{
    QTextCursor sel = textCursor();
    if (!sel.hasSelection()) return; // 无选定时直接返回

    QString selectedText = sel.selectedText();
    if (selectedText.isEmpty()) return;

    // 保存搜索词并使用查找高亮机制
    m_searchText = selectedText;
    m_searchFlags = QTextDocument::FindFlags();
    highlightAllMatches();

    // 定位到第一个匹配项
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size()) {
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
        highlightCurrentLine();
    }
}

/**
 * @brief 清除所有查找高亮
 */
void Editor::clearAllHighlights()
{
    clearFindHighlights();
}

/**
 * @brief 处理字体设置功能
 */
void Editor::handleFontSettings()
{
    bool ok;
    QFont currentFont = getEditorFont(); // 获取当前字体

    // 弹出字体选择对话框（已汉化）
    QFont newFont = QFontDialog::getFont(
        &ok,                 // 接收用户是否点击确定
        currentFont,         // 当前字体作为初始值
        this,                // 父窗口
        tr("文字设置")       // 对话框标题（已汉化）
    );

    if (ok) {
        // 应用新字体
        setEditorFont(newFont);

        // 在状态栏显示字体信息
        QMainWindow *mainWindow = qobject_cast<QMainWindow *>(window());
        if (mainWindow && mainWindow->statusBar()) {
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

/**
 * @brief 设置Tab替换为空格的功能
 * @param replace 是否替换
 * @param spaces 空格数量
 */
void Editor::setTabReplace(bool replace, int spaces)
{
    if (replace) {
        // 设置Tab键插入对应数量的空格
        setTabStopWidth(spaces * fontMetrics().width(' '));
    } else {
        // 恢复默认Tab行为（8个空格宽度）
        setTabStopWidth(8 * fontMetrics().width(' '));
    }
}

/**
 * @brief 处理注释功能（单行注释//）
 */
void Editor::handleComment()
{
    QTextCursor cursor = textCursor();
    bool hasSelection = cursor.hasSelection();

    if (hasSelection) {
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
        if (isCommented) {
            // 取消注释
            foreach (QString line, lines) {
                processedText += line.replace(QRegExp("^\\s*//"), "") + "\n";
            }
        } else {
            // 添加注释
            foreach (QString line, lines) {
                processedText += "//" + line + "\n";
            }
        }

        // 替换选中的文本（移除最后一个换行）
        cursor.insertText(processedText.left(processedText.length() - 1));
        setTextCursor(cursor);
    } else {
        // 注释当前行
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        if (line.trimmed().startsWith("//")) {
            // 取消注释
            line = line.replace(QRegExp("^\\s*//"), "");
        } else {
            // 添加注释
            line = "//" + line;
        }

        cursor.insertText(line);
    }

    highlightNewLines(); // 更新新增行标记
}

/**
 * @brief 高亮所有匹配项
 */
void Editor::highlightAllMatches()
{
    m_matchCursors.clear();
    if (m_searchText.isEmpty()) {
        m_currentMatchIndex = -1;
        highlightCurrentLine();
        return;
    }

    QTextCursor cursor(document());
    while (true) {
        cursor = document()->find(m_searchText, cursor, m_searchFlags);
        if (cursor.isNull()) break;
        m_matchCursors.push_back(cursor);
        cursor.setPosition(cursor.position()); // 防止空匹配循环
    }

    m_currentMatchIndex = m_matchCursors.isEmpty() ? -1 : 0;
    highlightCurrentLine();
}

/**
 * @brief 查找下一个匹配项
 */
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
    if (!target.isNull()) {
        setTextCursor(target);
        highlightCurrentLine();
    }
}

/**
 * @brief 查找上一个匹配项
 */
void Editor::findPrevious()
{
    if (m_searchText.isEmpty()) return;

    if (m_matchCursors.isEmpty()) {
        QTextCursor c = document()->find(m_searchText, textCursor(),
                                        m_searchFlags | QTextDocument::FindBackward);
        if (!c.isNull()) setTextCursor(c);
        return;
    }

    m_currentMatchIndex = (m_currentMatchIndex - 1 + m_matchCursors.size()) % m_matchCursors.size();
    QTextCursor target = m_matchCursors[m_currentMatchIndex];
    if (!target.isNull()) {
        setTextCursor(target);
        highlightCurrentLine();
    }
}

/**
 * @brief 清除查找高亮
 */
void Editor::clearFindHighlights()
{
    m_searchText.clear();
    m_matchCursors.clear();
    m_currentMatchIndex = -1;
    highlightCurrentLine();
}

/**
 * @brief 清除所有高亮
 */
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

/**
 * @brief 设置高亮相关动作
 * @param highlightAction 高亮动作
 * @param clearHighlightsAction 清除高亮动作
 */
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

/**
 * @brief 获取基础的额外选区（当前行+查找结果）
 * @return 额外选区列表
 */
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
