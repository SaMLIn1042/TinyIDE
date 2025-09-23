#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QAction>
#include <QFont>
#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextEdit>
#include <QSet>
#include <QVector>
#include <QTextCursor>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

// 直接在Editor头文件中定义语法高亮器类
class EditorSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit EditorSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;
    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;
    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat numberFormat;
};

class Editor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override;
    QString getCodeText() const;
    void setEditorFont(const QFont &font);
    QFont getEditorFont() const;
    void highlightNewLines();
    void setOriginalText(const QString &text);
    void findActionsFromMainWindow();
    void setHighlightActions(QAction *highlight, QAction *clear);
    void clearHighlights();
    void highlightSelection();
    void clearAllHighlights();
    void handleFind();
    void handleReplace();
    void lineCountExceeded();
    // 行号显示区域
    class LineNumberArea : public QWidget
    {
    public:
        LineNumberArea(Editor *editor) : QWidget(editor), codeEditor(editor) {}

        QSize sizeHint() const override
        {
            return QSize(codeEditor->lineNumberAreaWidth(), 0);
        }

    protected:
        void paintEvent(QPaintEvent *event) override
        {
            codeEditor->lineNumberAreaPaintEvent(event);
        }

    private:
        Editor *codeEditor;
    };
    int lineNumberAreaWidth();
    void lineNumberAreaPaintEvent(QPaintEvent *event);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void loadChineseTranslation();
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void handleUndo();
    void handleCut();
    void handleCopy();
    void handlePaste();
    void updatePasteState();
    void findNext();
    void findPrevious();
    void clearFindHighlights();
    void handleInsert();
    void handleFontSettings();
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void onTextChanged();
    void setTabReplace(bool replace, int spaces = 4);
    void handleComment();
    void highlightMatchingBracket();

private:
    QAction *undoAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *pasteAction;
    QAction *findAction;
    QAction *replaceAction;
    QAction *insertAction;
    QAction *fontAction;
    QAction *highlightSelectionAction{nullptr};
    QAction *clearHighlightsAction{nullptr};
    LineNumberArea *lineNumberArea;
    QString m_originalText;
    QSet<int> m_newLineNumbers;
    QString m_searchText;
    QTextDocument::FindFlags m_searchFlags;
    QVector<QTextCursor> m_matchCursors;
    int m_currentMatchIndex = -1;

    EditorSyntaxHighlighter *highlighter;

    void setupConnections();
    void updateActionStates();
    void replaceCurrent(const QString &searchText, const QString &replaceText);
    void replaceAll(const QString &searchText, const QString &replaceText);
    QList<QTextEdit::ExtraSelection> baseExtraSelections() const;
    QList<QTextEdit::ExtraSelection> m_selectionExtraSelections;
    void highlightAllMatches();
    QHash<QChar, QChar> m_matchingPairs;
    int findMatchingBracket(int startPos, QChar bracket, QChar matchBracket, int direction);
    void highlightBracketPair(int pos1, int pos2);
    void updateBracketHighlight();
    void clearBracketHighlight();
    QList<QTextEdit::ExtraSelection> m_bracketSelections;
    QString calculateIndentation() const;
    int getIndentationLevel() const;
    void checkAndClearBracketHighlight();//及时清除匹配括号高亮
};

#endif // EDITOR_H
