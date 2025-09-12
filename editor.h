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

class Editor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    ~Editor() override = default;

    QString getCodeText() const;
    void setEditorFont(const QFont &font); // 设置编辑器字体
    QFont getEditorFont() const; // 获取当前字体

    void highlightNewLines();
    void setOriginalText(const QString &text);
    void findActionsFromMainWindow();
    void setHighlightActions(QAction *highlight, QAction *clear);//高亮
    void clearHighlights();

    // 行号显示区域
    class LineNumberArea : public QWidget
    {
    public:
        LineNumberArea(Editor *editor) : QWidget(editor), codeEditor(editor){}

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

private slots:
    void handleUndo();
    void handleCut();
    void handleCopy();
    void handlePaste();
    void updatePasteState();

    void handleFind();
    void handleReplace();
    void findNext();
    void findPrevious();
    void clearFindHighlights();

    void handleInsert();
    void handleFontSettings(); // 新增：处理字体设置
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void onTextChanged();
    void setTabReplace(bool replace, int spaces = 4); // 仅保留一次声明
    void handleComment();

private:
    QAction *undoAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *pasteAction;
    QAction *findAction;
    QAction *replaceAction;
    QAction *insertAction;
    QAction *fontAction; // 新增：字体设置动作
    QAction *highlightSelectionAction{nullptr};//高亮相关
    QAction *clearHighlightsAction{nullptr};//高亮相关

    LineNumberArea *lineNumberArea;
    QString m_originalText;  // 用于跟踪新增内容的原始文本
    QSet<int> m_newLineNumbers;  // 新增行号集合

    //查找相关状态
    QString m_searchText;
    QTextDocument::FindFlags m_searchFlags;
    QVector<QTextCursor> m_matchCursors;
    int m_currentMatchIndex = -1;


    void setupConnections();
    void updateActionStates();
    void replaceCurrent(const QString &searchText, const QString &replaceText);
    void replaceAll(const QString &searchText, const QString &replaceText);

    QList<QTextEdit::ExtraSelection> baseExtraSelections() const;
    QList<QTextEdit::ExtraSelection> m_selectionExtraSelections;

    void highlightAllMatches();

    void highlightSelection();//高亮相关
    void clearAllHighlights();//高亮相关
    QHash<QChar, QChar> m_matchingPairs;    // 新增：存储成对符号
};

#endif // EDITOR_H
