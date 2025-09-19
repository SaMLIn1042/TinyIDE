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

// 初始化编辑器组件和状态
Editor::Editor(QWidget *parent) : QPlainTextEdit(parent),
                                  undoAction(nullptr), cutAction(nullptr),
                                  copyAction(nullptr), pasteAction(nullptr),
                                  findAction(nullptr), replaceAction(nullptr),
                                  insertAction(nullptr), fontAction(nullptr),
                                  lineNumberArea(new LineNumberArea(this))
{
    // 加载中文本地化支持
    loadChineseTranslation();

    setUndoRedoEnabled(true); // 启用撤销/重做

    // 设置Tab为4个空格
    setTabReplace(true, 4);
    // 关联主窗口动作
    findActionsFromMainWindow();
    // 建立信号槽连接
    setupConnections();
    // 更新动作状态
    updateActionStates();

    // 初始化原始文本内容
    m_originalText = toPlainText();

    // 初始化符号配对映射
    m_matchingPairs.insert('(', ')');
    m_matchingPairs.insert('{', '}');
    m_matchingPairs.insert('[', ']');
    m_matchingPairs.insert('<', '>');
    m_matchingPairs.insert('\'', '\'');
    m_matchingPairs.insert('"', '"');

    // 连接行号和光标等相关信号
    connect(this, &Editor::blockCountChanged, this, &Editor::updateLineNumberAreaWidth);
    connect(this, &Editor::updateRequest, this, &Editor::updateLineNumberArea);
    connect(this, &Editor::cursorPositionChanged, this, &Editor::highlightCurrentLine);
    connect(this, &Editor::textChanged, this, &Editor::onTextChanged);
    connect(this, &Editor::cursorPositionChanged, this, &Editor::highlightMatchingBracket);
    connect(this, &QPlainTextEdit::textChanged, this, &Editor::checkLineCountLimit);

    // 初始化界面状态
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    highlightNewLines();

    // 初始化语法高亮器
    highlighter = new EditorSyntaxHighlighter(document());
}

// 计算当前行的缩进级别
int Editor::getIndentationLevel() const
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString line = cursor.selectedText();

    int level = 0;
    int tabWidth = 4; // 缩进单位：4空格
    int spaceCount = 0;

    // 计算缩进：空格和Tab
    foreach (QChar c, line)
    {
        if (c == ' ')
        {
            spaceCount++;
            if (spaceCount % tabWidth == 0)
            {
                level++;
            }
        }
        else if (c == '\t')
        {
            level++;
            spaceCount = 0;
        }
        else
        {
            break;
        }
    }

    return level;
}

// 计算缩进字符串
QString Editor::calculateIndentation() const
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString currentLine = cursor.selectedText();

    // 基础缩进级别
    int indentLevel = getIndentationLevel();

    // 遇到{增加一级缩进
    if (currentLine.contains('{') && !currentLine.contains('}'))
    {
        indentLevel++;
    }

    // 生成缩进空格字符串
    int tabWidth = 4;
    return QString(tabWidth * indentLevel, ' ');
}

// 加载Qt中文翻译文件
void Editor::loadChineseTranslation()
{
    // 加载Qt基础翻译
    QTranslator *qtTranslator = new QTranslator(qApp);
    if (qtTranslator->load("qt_zh_CN.qm", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    {
        qApp->installTranslator(qtTranslator);
    }

    // 加载Qt UI翻译
    QTranslator *qtBaseTranslator = new QTranslator(qApp);
    if (qtBaseTranslator->load("qtbase_zh_CN.qm", QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    {
        qApp->installTranslator(qtBaseTranslator);
    }
}

// 处理键盘输入：自动补全、缩进等
void Editor::keyPressEvent(QKeyEvent *event)
{
    // 回车键：自动缩进
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        QString indent = calculateIndentation();
        QTextCursor cursor = textCursor();
        cursor.insertText("\n" + indent);
        setTextCursor(cursor);
        event->accept();
        return;
    }
    // 右花括号：减少缩进
    else if (event->key() == Qt::Key_BraceRight)
    {
        int indentLevel = getIndentationLevel();
        if (indentLevel > 0)
            indentLevel--;

        int tabWidth = tabStopWidth() / fontMetrics().width(' ');
        QString indent = QString(tabWidth * indentLevel, ' ');

        QTextCursor cursor = textCursor();
        cursor.insertText("}");

        // 调整行首缩进
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
    // 退格键：删除成对符号
    else if (event->key() == Qt::Key_Backspace)
    {
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection())
        {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        // 检查并删除成对符号
        int pos = cursor.position();
        if (pos > 0)
        {
            cursor.setPosition(pos - 1);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            QString leftChar = cursor.selectedText();

            if (!leftChar.isEmpty() && m_matchingPairs.contains(leftChar[0]))
            {
                QChar rightChar = m_matchingPairs[leftChar[0]];
                if (pos < document()->characterCount())
                {
                    cursor.setPosition(pos);
                    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                    QString actualRightChar = cursor.selectedText();
                    if (actualRightChar[0] == rightChar)
                    {
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

    // 符号自动补全
    QString inputText = event->text();
    if (!inputText.isEmpty())
    {
        QChar inputChar = inputText.at(0);
        if (m_matchingPairs.contains(inputChar) &&
            !((inputChar == '\'' || inputChar == '"') && textCursor().hasSelection()))
        {
            QChar matchingChar = m_matchingPairs[inputChar];
            QTextCursor cursor = textCursor();
            int originalPos = cursor.position();

            // 包裹选中文本或插入符号对
            if ((inputChar == '\'' || inputChar == '"') && cursor.hasSelection())
            {
                QString selectedText = cursor.selectedText();
                cursor.removeSelectedText();
                cursor.insertText(inputChar + selectedText + matchingChar);
                cursor.setPosition(originalPos + selectedText.length() + 2);
            }
            else
            {
                cursor.insertText(inputChar);
                cursor.insertText(matchingChar);
                cursor.setPosition(originalPos + 1);
            }

            setTextCursor(cursor);
            event->accept();
            return;
        }
    }

    // 默认处理
    QPlainTextEdit::keyPressEvent(event);
    highlightMatchingBracket(); // 更新括号高亮
}

// 计算行号区域宽度
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
    painter.fillRect(event->rect(), Qt::lightGray); // 背景色

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int)blockBoundingRect(block).height();

    // 绘制所有可见行号
    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::black);

            // 新增行显示为红色
            if (m_newLineNumbers.contains(blockNumber + 1))
                painter.setPen(Qt::red);

            painter.drawText(0, top, lineNumberArea->width() - 3,
                             fontMetrics().height(), Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)blockBoundingRect(block).height();
        ++blockNumber;
    }
}

// 文本变化回调
void Editor::onTextChanged()
{
    // 检查行数是否超过2000
    if (document()->lineCount() > 2000) {
        emit lineCountExceeded();
    }
    highlightNewLines();
}

// 高亮新增行（与原始文本对比）
void Editor::highlightNewLines()
{
    m_newLineNumbers.clear();
    QString currentText = toPlainText();
    QStringList originalLines = m_originalText.split('\n');
    QStringList currentLines = currentText.split('\n');

    // 使用LCS算法标记新增行
    int m = originalLines.size();
    int n = currentLines.size();
    QVector<QVector<int>> lcs(m + 1, QVector<int>(n + 1, 0));

    // 计算最长公共子序列
    for (int i = 1; i <= m; ++i)
        for (int j = 1; j <= n; ++j)
            lcs[i][j] = (originalLines[i - 1] == currentLines[j - 1]) ? lcs[i - 1][j - 1] + 1 : qMax(lcs[i - 1][j], lcs[i][j - 1]);

    // 标记匹配的行
    QSet<int> matchedOriginalLines, matchedCurrentLines;
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
            i--;
        else
            j--;
    }

    // 标记未匹配的当前行为新增行
    for (int k = 0; k < currentLines.size(); ++k)
        if (!matchedCurrentLines.contains(k))
            m_newLineNumbers.insert(k + 1);

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

// 高亮当前行和查找匹配
void Editor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    // 当前行高亮
    if (!isReadOnly())
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(Qt::yellow).lighter(160));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    // 查找匹配高亮
    for (int i = 0; i < m_matchCursors.size(); ++i)
    {
        const QTextCursor &mc = m_matchCursors[i];
        if (mc.isNull())
            continue;

        QTextEdit::ExtraSelection matchSel;
        matchSel.format.setBackground(QColor(Qt::cyan).lighter(180));
        matchSel.cursor = mc;
        extraSelections.append(matchSel);
    }

    // 当前匹配项高亮
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
    {
        QTextCursor cur = m_matchCursors[m_currentMatchIndex];
        if (!cur.isNull())
        {
            QTextEdit::ExtraSelection curSel;
            curSel.format.setBackground(QColor(Qt::blue).lighter(170));
            curSel.cursor = cur;
            extraSelections.append(curSel);
        }
    }

    // 添加括号高亮
    extraSelections.append(m_bracketSelections);
    setExtraSelections(extraSelections);
}

// 设置原始文本（用于对比新增内容）
void Editor::setOriginalText(const QString &text)
{
    m_originalText = text;
    highlightNewLines();
}

// 获取编辑器纯文本内容
QString Editor::getCodeText() const
{
    return toPlainText();
}

// 设置编辑器字体
void Editor::setEditorFont(const QFont &font)
{
    setFont(font);
    setTabReplace(true, 4);       // 更新Tab宽度
    updateLineNumberAreaWidth(0); // 更新行号区域
}

// 获取当前编辑器字体
QFont Editor::getEditorFont() const
{
    return font();
}

// 从主窗口查找并关联编辑动作
void Editor::findActionsFromMainWindow()
{
    QMainWindow *mainWindow = nullptr;
    QWidget *currentParent = parentWidget();

    // 向上查找主窗口
    while (currentParent && !mainWindow)
    {
        mainWindow = qobject_cast<QMainWindow *>(currentParent);
        currentParent = currentParent->parentWidget();
    }

    // 尝试应用顶层窗口
    if (!mainWindow)
    {
        foreach (QWidget *widget, QApplication::topLevelWidgets())
        {
            mainWindow = qobject_cast<QMainWindow *>(widget);
            if (mainWindow)
                break;
        }
    }

    if (!mainWindow)
    {
        qDebug() << "警告：未找到主窗口，编辑器动作可能无法正常工作";
        return;
    }

    // 匹配主窗口中的动作
    QList<QAction *> allActions = mainWindow->findChildren<QAction *>();
    foreach (QAction *action, allActions)
    {
        const QString &objName = action->objectName();
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

    // 调试输出
    qDebug() << "动作匹配情况：";
    qDebug() << "撤销动作: " << (undoAction ? "找到" : "未找到");
    qDebug() << "剪切动作: " << (cutAction ? "找到" : "未找到");
    qDebug() << "复制动作: " << (copyAction ? "找到" : "未找到");
    qDebug() << "粘贴动作: " << (pasteAction ? "找到" : "未找到");
    qDebug() << "查找动作: " << (findAction ? "找到" : "未找到");
    qDebug() << "替换动作: " << (replaceAction ? "找到" : "未找到");
    qDebug() << "插入动作: " << (insertAction ? "找到" : "未找到");
    qDebug() << "字体动作: " << (fontAction ? "找到" : "未找到");

    setupConnections();
}

// 建立动作与编辑器功能的连接
void Editor::setupConnections()
{
    // 连接编辑动作
    if (undoAction)
        connect(undoAction, &QAction::triggered, this, &Editor::handleUndo);
    if (cutAction)
        connect(cutAction, &QAction::triggered, this, &Editor::handleCut);
    if (copyAction)
        connect(copyAction, &QAction::triggered, this, &Editor::handleCopy);
    if (pasteAction)
        connect(pasteAction, &QAction::triggered, this, &Editor::handlePaste);
    // NOTE: do NOT connect global actions like actionFind/actionReplace here.
//    if (findAction)
//        connect(findAction, &QAction::triggered, this, &Editor::handleFind);
//    if (replaceAction)
//        connect(replaceAction, &QAction::triggered, this, &Editor::handleReplace);
    if (insertAction)
        connect(insertAction, &QAction::triggered, this, &Editor::handleInsert);
    if (fontAction)
        connect(fontAction, &QAction::triggered, this, &Editor::handleFontSettings);
//    if (highlightSelectionAction)
//        connect(highlightSelectionAction, &QAction::triggered, this, &Editor::highlightSelection);
//    if (clearHighlightsAction)
//        connect(clearHighlightsAction, &QAction::triggered, this, &Editor::clearAllHighlights);

    // 连接状态更新信号
    connect(this, &QPlainTextEdit::undoAvailable, undoAction, &QAction::setEnabled);
    connect(this, &QPlainTextEdit::copyAvailable, cutAction, &QAction::setEnabled);
    connect(this, &QPlainTextEdit::copyAvailable, copyAction, &QAction::setEnabled);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &Editor::updatePasteState);
    connect(this, &QPlainTextEdit::textChanged, this, &Editor::updateActionStates);

    // 添加快捷键动作
    QAction *commentAction = new QAction(tr("注释"), this);
    commentAction->setShortcut(QKeySequence("Ctrl+/"));
    commentAction->setToolTip(tr("注释/取消注释所选行 (Ctrl+/)"));
    connect(commentAction, &QAction::triggered, this, &Editor::handleComment);
    addAction(commentAction);

    QAction *findNextAction = new QAction(tr("查找下一个"), this);
    findNextAction->setShortcut(QKeySequence::FindNext);
    findNextAction->setToolTip(tr("查找下一个匹配项 (F3)"));
    connect(findNextAction, &QAction::triggered, this, &Editor::findNext);
    addAction(findNextAction);

    QAction *findPrevAction = new QAction(tr("查找上一个"), this);
    findPrevAction->setShortcut(QKeySequence::FindPrevious);
    findPrevAction->setToolTip(tr("查找上一个匹配项 (Shift+F3)"));
    connect(findPrevAction, &QAction::triggered, this, &Editor::findPrevious);
    addAction(findPrevAction);
}

// 更新动作状态
void Editor::updateActionStates()
{
    if (undoAction)
        undoAction->setEnabled(document()->isUndoAvailable());

    bool hasSelection = textCursor().hasSelection();
    if (cutAction)
        cutAction->setEnabled(hasSelection);
    if (copyAction)
        copyAction->setEnabled(hasSelection);

    updatePasteState();
}

// 更新粘贴状态
void Editor::updatePasteState()
{
    if (pasteAction)
        pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
}

// 执行撤销
void Editor::handleUndo()
{
    undo();
    updateActionStates();
    highlightNewLines();
}

// 执行剪切
void Editor::handleCut()
{
    cut();
    updateActionStates();
}

// 执行复制
void Editor::handleCopy()
{
    copy();
    updateActionStates();
}

// 执行粘贴（Tab替换为空格）
void Editor::handlePaste()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();

    // 替换Tab为空格
    int tabWidth = tabStopWidth() / fontMetrics().width(' ');
    text.replace("\t", QString(" ").repeated(tabWidth));

    QTextCursor cursor = textCursor();
    cursor.insertText(text);
    setTextCursor(cursor);
    updateActionStates();
}

// 处理查找
void Editor::handleFind()
{
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("查找"),
                                               tr("请输入要查找的内容:"), QLineEdit::Normal,
                                               m_searchText, &ok);
    // 规范化换行符
    searchText.replace(QChar::ParagraphSeparator, '\n');
    searchText.replace("\r\n", "\n");
    searchText.replace('\r', '\n');

    if (!ok || searchText.isEmpty())
        return;

    m_searchText = searchText;
    m_searchFlags = QTextDocument::FindFlags();
    highlightAllMatches();

    // 跳转到第一个匹配
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
}

// 处理替换
void Editor::handleReplace()
{
    bool ok;
    // 获取查找文本
    QString searchText = QInputDialog::getText(this, tr("替换"),
                                               tr("请输入要查找的内容:"), QLineEdit::Normal,
                                               "", &ok);
    searchText.replace(QChar::ParagraphSeparator, '\n');
    searchText.replace("\r\n", "\n");
    searchText.replace('\r', '\n');
    if (!ok || searchText.isEmpty())
        return;

    // 获取替换文本
    QString replaceText = QInputDialog::getText(this, tr("替换"),
                                                tr("请输入替换文本:"), QLineEdit::Normal,
                                                "", &ok);
    if (!ok)
        return;

    // 选择替换方式
    QStringList options;
    options << tr("替换当前匹配项") << tr("替换所有匹配项");
    QString choice = QInputDialog::getItem(this, tr("替换选项"),
                                           tr("请选择操作:"), options, 0, false, &ok);
    if (!ok)
        return;

    // 执行替换
    if (choice == options[0])
        replaceCurrent(searchText, replaceText);
    else
        replaceAll(searchText, replaceText);

    highlightNewLines();
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
            qDebug() << "未找到匹配的文本: " << searchText;
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

// 处理文本插入
void Editor::handleInsert()
{
    bool ok;
    QString insertText = QInputDialog::getText(this, tr("插入文本"),
                                               tr("请输入要插入的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || insertText.isEmpty())
        return;

    QTextCursor cursor = textCursor();
    cursor.insertText(insertText);
    setTextCursor(cursor);
}

// 高亮选中文本的所有匹配项
void Editor::highlightSelection()
{
    QTextCursor sel = textCursor();
    if (!sel.hasSelection())
        return;

    QString selectedText = sel.selectedText();
    if (selectedText.isEmpty())
        return;

    // 规范化换行符
    selectedText.replace(QChar::ParagraphSeparator, '\n');
    selectedText.replace("\r\n", "\n");
    selectedText.replace('\r', '\n');

    m_searchText = selectedText;
    m_searchFlags = QTextDocument::FindFlags();
    highlightAllMatches();

    // 跳转到第一个匹配
    if (m_currentMatchIndex >= 0 && m_currentMatchIndex < m_matchCursors.size())
    {
        setTextCursor(m_matchCursors[m_currentMatchIndex]);
        highlightCurrentLine();
    }
}

// 清除所有高亮
void Editor::clearAllHighlights()
{
    clearFindHighlights();
}

// 处理字体设置
void Editor::handleFontSettings()
{
    bool ok;
    QFont currentFont = getEditorFont();
    QFont newFont = QFontDialog::getFont(&ok, currentFont, this, tr("文字设置"));

    if (ok)
    {
        setEditorFont(newFont);
        // 状态栏提示
        QMainWindow *mainWindow = qobject_cast<QMainWindow *>(window());
        if (mainWindow && mainWindow->statusBar())
            mainWindow->statusBar()->showMessage(tr("字体已更新: %1 %2点").arg(newFont.family()).arg(newFont.pointSize()), 3000);

        qDebug() << "字体已更新为: " << newFont.family() << ", 大小: " << newFont.pointSize() << "点";
    }
}

// 设置Tab替换选项
void Editor::setTabReplace(bool replace, int spaces)
{
    if (replace)
        setTabStopWidth(spaces * fontMetrics().width(' '));
    else
        setTabStopWidth(8 * fontMetrics().width(' '));
}

// 处理注释/取消注释
void Editor::handleComment()
{
    QTextCursor cursor = textCursor();
    bool hasSelection = cursor.hasSelection();

    if (hasSelection)
    {
        // 处理多行注释
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();
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

        // 检查是否已注释
        bool isCommented = lines.first().trimmed().startsWith("//");
        QString processedText;

        if (isCommented)
        {
            // 移除注释
            foreach (QString line, lines)
                processedText += line.replace(QRegExp("^\\s*//"), "") + "\n";
        }
        else
        {
            // 添加注释
            foreach (QString line, lines)
                processedText += "//" + line + "\n";
        }

        cursor.insertText(processedText.left(processedText.length() - 1));
        setTextCursor(cursor);
    }
    else
    {
        // 单行注释
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        if (line.trimmed().startsWith("//"))
            line = line.replace(QRegExp("^\\s*//"), "");
        else
            line = "//" + line;

        cursor.insertText(line);
    }

    highlightNewLines();
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

    QString search = m_searchText;
    search.replace(QChar::ParagraphSeparator, '\n');
    search.replace("\r\n", "\n");
    search.replace('\r', '\n');

    // 在全文中查找所有匹配
    QString docText = toPlainText();
    int pos = docText.indexOf(search, 0);
    while (pos != -1)
    {
        QTextCursor c(document());
        c.setPosition(pos);
        c.setPosition(pos + search.length(), QTextCursor::KeepAnchor);
        m_matchCursors.push_back(c);
        pos = docText.indexOf(search, pos + qMax(1, search.length()));
    }

    m_currentMatchIndex = m_matchCursors.isEmpty() ? -1 : 0;
    highlightCurrentLine();
}

// 查找下一个匹配
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

// 查找上一个匹配
void Editor::findPrevious()
{
    if (m_searchText.isEmpty())
        return;

    if (m_matchCursors.isEmpty())
    {
        QTextCursor c = document()->find(m_searchText, textCursor(), m_searchFlags | QTextDocument::FindBackward);
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
    m_matchCursors.clear();
    m_searchText.clear();
    m_currentMatchIndex = -1;
    highlightAllMatches();
    m_selectionExtraSelections.clear();
    setExtraSelections(baseExtraSelections());
}

// 设置高亮动作
void Editor::setHighlightActions(QAction *highlightAction, QAction *clearHighlightsAction)
{
    this->highlightSelectionAction = highlightAction;
    this->clearHighlightsAction = clearHighlightsAction;

    if (highlightSelectionAction)
        connect(highlightSelectionAction, &QAction::triggered, this, &Editor::highlightSelection);
    if (clearHighlightsAction)
        connect(clearHighlightsAction, &QAction::triggered, this, &Editor::clearHighlights);
}

// 获取基础高亮选区
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

    // 手动选中高亮
    extraSelections.append(m_selectionExtraSelections);
    return extraSelections;
}

// 高亮匹配括号
void Editor::highlightMatchingBracket()
{
    QTextCursor cursor = textCursor();
    int position = cursor.position();
    QTextDocument *doc = document();

    // 检查当前字符和前一个字符
    QChar currentChar, prevChar;
    if (position > 0)
        prevChar = doc->characterAt(position - 1);
    if (position < doc->characterCount())
        currentChar = doc->characterAt(position);

    // 左括号匹配
    if (m_matchingPairs.contains(prevChar))
    {
        QChar matchChar = m_matchingPairs[prevChar];
        int matchPos = findMatchingBracket(position - 1, prevChar, matchChar, 1);
        if (matchPos != -1)
            highlightBracketPair(position - 1, matchPos);
    }
    // 右括号匹配
    else if (m_matchingPairs.values().contains(currentChar))
    {
        QChar matchChar;
        for (auto it = m_matchingPairs.begin(); it != m_matchingPairs.end(); ++it)
            if (it.value() == currentChar)
            {
                matchChar = it.key();
                break;
            }
        int matchPos = findMatchingBracket(position, currentChar, matchChar, -1);
        if (matchPos != -1)
            highlightBracketPair(matchPos, position);
    }
}

// 查找匹配括号
int Editor::findMatchingBracket(int startPos, QChar bracket, QChar matchBracket, int direction)
{
    QTextDocument *doc = document();
    int depth = 1;
    int currentPos = startPos + direction;

    // 处理字符串内的括号
    static QSet<QChar> quotes = {'"', '\''};
    static bool inString = false;
    static QChar currentQuote;

    while (currentPos >= 0 && currentPos < doc->characterCount())
    {
        QChar c = doc->characterAt(currentPos);

        // 字符串检测
        if (quotes.contains(c))
        {
            if (!inString)
            {
                inString = true;
                currentQuote = c;
            }
            else if (c == currentQuote)
                inString = false;
            currentPos += direction;
            continue;
        }
        if (inString)
        {
            currentPos += direction;
            continue;
        }

        // 括号匹配
        if (c == bracket)
            depth++;
        else if (c == matchBracket)
        {
            depth--;
            if (depth == 0)
                return currentPos;
        }

        currentPos += direction;
    }

    return -1;
}

// 高亮括号对
void Editor::highlightBracketPair(int pos1, int pos2)
{
    QTextCharFormat format;
    format.setBackground(QColor(255, 255, 153)); // 浅黄背景
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

    m_bracketSelections.clear();
    m_bracketSelections.append(selection1);
    m_bracketSelections.append(selection2);
    updateBracketHighlight();
}

// 清除括号高亮
void Editor::clearBracketHighlight()
{
    if (!m_bracketSelections.isEmpty())
    {
        m_bracketSelections.clear();
        updateBracketHighlight();
    }
}

// 更新括号高亮显示
void Editor::updateBracketHighlight()
{
    QList<QTextEdit::ExtraSelection> selections = extraSelections();

    // 移除旧括号高亮
    for (int i = selections.size() - 1; i >= 0; --i)
        if (selections[i].format.background().color() == QColor(255, 255, 153))
            selections.removeAt(i);

    selections.append(m_bracketSelections);
    setExtraSelections(selections);
}

// 鼠标滚轮事件：Ctrl+滚轮调整字体大小
void Editor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        QFont currentFont = font();
        int fontSize = currentFont.pointSize();

        // 调整字体大小
        if (event->angleDelta().y() > 0)
            fontSize += 1;
        else
            fontSize -= 1;

        fontSize = qMax(6, qMin(72, fontSize));
        currentFont.setPointSize(fontSize);
        setFont(currentFont);
        setTabReplace(true, 4); // 更新Tab宽度
        event->accept();
    }
    else
    {
        QPlainTextEdit::wheelEvent(event);
    }
}

// 语法高亮器构造函数
EditorSyntaxHighlighter::EditorSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 设置关键字格式
    keywordFormat.setForeground(Qt::darkBlue);
    keywordFormat.setFontWeight(QFont::Bold);

    // C++关键字列表
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
        "\\bnew\\b", "\\bthis\\b", "\\bsizeof\\b", "\\btrue\\b", "\\bfalse\\b"};

    // 添加关键字规则
    for (const QString &pattern : keywordPatterns)
        highlightingRules.append({QRegularExpression(pattern), keywordFormat});

    // 类名格式（Qt类）
    classFormat.setForeground(Qt::darkMagenta);
    classFormat.setFontWeight(QFont::Bold);
    highlightingRules.append({QRegularExpression("\\bQ[A-Za-z]+\\b"), classFormat});

    // 函数格式
    functionFormat.setForeground(Qt::darkCyan);
    highlightingRules.append({QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()"), functionFormat});

    // 字符串格式
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

// 应用语法高亮规则
void EditorSyntaxHighlighter::highlightBlock(const QString &text)
{
    // 应用单行规则
    for (const HighlightingRule &rule : qAsConst(highlightingRules))
    {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext())
        {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // 处理多行注释
    setCurrentBlockState(0);
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(commentStartExpression);

    while (startIndex >= 0)
    {
        QRegularExpressionMatch match = commentEndExpression.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;

        if (endIndex == -1)
        {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        }
        else
        {
            commentLength = endIndex - startIndex + match.capturedLength();
        }

        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
    }
}

bool Editor::isLineCountValid() const {
    return document()->blockCount() <= 2000;
}

void Editor::checkLineCountLimit() {
    if (document()->blockCount() > 2000) {
        // 阻止进一步输入并提示用户
        undo(); // 撤销最后一次操作（即超限的输入）
        emit lineCountExceeded();
    }
}

// 析构函数：清理资源
Editor::~Editor()
{
    if (highlighter)
    {
        delete highlighter;
        highlighter = nullptr;
    }
}
