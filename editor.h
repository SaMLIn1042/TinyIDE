#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QAction>

class Editor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    QString getCodeText() const;

private slots:
    void handleUndo();
    void handleCut();
    void handleCopy();
    void handlePaste();
    void updatePasteState();

private:
    QAction *undoAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *pasteAction;

    // 必须在这里声明所有私有函数
    void findActionsFromMainWindow();  // 关键：添加这个声明
    void setupConnections();
    void updateActionStates();
};

#endif // EDITOR_H
