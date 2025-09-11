#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QAction>
#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextEdit>
#include <QSet>

class Editor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    QString getCodeText() const;

    void highlightNewLines();
    void setOriginalText(const QString &text);

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

private slots:
    void handleUndo();
    void handleCut();
    void handleCopy();
    void handlePaste();
    void updatePasteState();
    void handleFind();
    void handleReplace();
    void handleInsert();
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);
    void onTextChanged();


private:
    QAction *undoAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *pasteAction;
    QAction *findAction;
    QAction *replaceAction;
    QAction *insertAction;
    LineNumberArea *lineNumberArea;
    QString m_originalText;  // 用于跟踪新增内容的原始文本
    QSet<int> m_newLineNumbers;  // 新增行号集合


    void findActionsFromMainWindow();
    void setupConnections();
    void updateActionStates();

    void replaceCurrent(const QString &searchText, const QString &replaceText);
    void replaceAll(const QString &searchText, const QString &replaceText);
};

#endif // EDITOR_H
