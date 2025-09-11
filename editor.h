#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>
#include <QAction>
#include <QFont>

class Editor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit Editor(QWidget *parent = nullptr);
    QString getCodeText() const;
    void setEditorFont(const QFont &font); // 设置编辑器字体
    QFont getEditorFont() const; // 获取当前字体

private slots:
    void handleUndo();
    void handleCut();
    void handleCopy();
    void handlePaste();
    void updatePasteState();
    void handleFind();
    void handleReplace();
    void handleInsert();
    void handleFontSettings(); // 新增：处理字体设置

private:
    QAction *undoAction;
    QAction *cutAction;
    QAction *copyAction;
    QAction *pasteAction;
    QAction *findAction;
    QAction *replaceAction;
    QAction *insertAction;
    QAction *fontAction; // 新增：字体设置动作

    void findActionsFromMainWindow();
    void setupConnections();
    void updateActionStates();
    void replaceCurrent(const QString &searchText, const QString &replaceText);
    void replaceAll(const QString &searchText, const QString &replaceText);
    void setTabReplace(bool replace, int spaces = 4);
};

#endif // EDITOR_H
