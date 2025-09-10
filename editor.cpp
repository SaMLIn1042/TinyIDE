#include "editor.h"
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QInputDialog>
#include <QTextCursor>
#include <QTextDocument>

Editor::Editor(QWidget *parent) : QPlainTextEdit(parent),
    undoAction(nullptr), cutAction(nullptr),
    copyAction(nullptr), pasteAction(nullptr),
    findAction(nullptr), replaceAction(nullptr), insertAction(nullptr)
{
    setUndoRedoEnabled(true);
    findActionsFromMainWindow();
    setupConnections();
    updateActionStates();
}

QString Editor::getCodeText() const {
    return toPlainText();
}

void Editor::findActionsFromMainWindow() {
    QMainWindow *mainWindow = qobject_cast<QMainWindow*>(window());
    if (!mainWindow) {
        qDebug() << "错误：未找到主窗口";
        return;
    }

    QList<QAction*> allActions = mainWindow->findChildren<QAction*>();
    foreach (QAction *action, allActions) {
        const QString &objName = action->objectName();
        if (objName == "actionUndo") {
            undoAction = action;
        } else if (objName == "actionCut") {
            cutAction = action;
        } else if (objName == "actionCopy") {
            copyAction = action;
        } else if (objName == "actionPaste") {
            pasteAction = action;
        } else if (objName == "actionFind") {
            findAction = action;
        } else if (objName == "actionReplace") {
            replaceAction = action;
        } else if (objName == "actionInsert") {
            insertAction = action;
        }
    }

    qDebug() << "动作匹配情况：";
    qDebug() << "actionUndo: " << (undoAction ? "找到" : "未找到");
    qDebug() << "actionCut: " << (cutAction ? "找到" : "未找到");
    qDebug() << "actionCopy: " << (copyAction ? "找到" : "未找到");
    qDebug() << "actionPaste: " << (pasteAction ? "找到" : "未找到");
    qDebug() << "actionFind: " << (findAction ? "找到" : "未找到");
    qDebug() << "actionReplace: " << (replaceAction ? "找到" : "未找到");
    qDebug() << "actionInsert: " << (insertAction ? "找到" : "未找到");
}

void Editor::setupConnections() {
    if (undoAction) {
        disconnect(undoAction, 0, 0, 0);
        connect(undoAction, &QAction::triggered, this, &Editor::handleUndo);
        connect(this, &QPlainTextEdit::undoAvailable, undoAction, &QAction::setEnabled);
    }

    if (cutAction) {
        disconnect(cutAction, 0, 0, 0);
        connect(cutAction, &QAction::triggered, this, &Editor::handleCut);
        connect(this, &QPlainTextEdit::copyAvailable, cutAction, &QAction::setEnabled);
    }

    if (copyAction) {
        disconnect(copyAction, 0, 0, 0);
        connect(copyAction, &QAction::triggered, this, &Editor::handleCopy);
        connect(this, &QPlainTextEdit::copyAvailable, copyAction, &QAction::setEnabled);
    }

    if (pasteAction) {
        disconnect(pasteAction, 0, 0, 0);
        connect(pasteAction, &QAction::triggered, this, &Editor::handlePaste);
        connect(QApplication::clipboard(), &QClipboard::dataChanged,
                this, &Editor::updatePasteState);
    }

    if (findAction) {
        disconnect(findAction, 0, 0, 0);
        findAction->setShortcut(QKeySequence::Find);   // 设置标准查找快捷键 (通常是 Ctrl+F)
        findAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(findAction, &QAction::triggered, this, &Editor::handleFind);
    }

    if (replaceAction) {
        disconnect(replaceAction, 0, 0, 0);
        replaceAction->setShortcut(QKeySequence::Replace);  // Ctrl+H
        replaceAction->setShortcutContext(Qt::ApplicationShortcut);
        connect(replaceAction, &QAction::triggered, this, &Editor::handleReplace);
    }

    if (insertAction) {
        disconnect(insertAction, 0, 0, 0);
        connect(insertAction, &QAction::triggered, this, &Editor::handleInsert);
    }
}

// 修复：使用document()->isUndoAvailable()检查撤销状态
void Editor::updateActionStates() {
    if (undoAction) {
        // 正确的撤销状态检查方法
        undoAction->setEnabled(document()->isUndoAvailable());
    }
    bool hasSelection = textCursor().hasSelection();
    if (cutAction) cutAction->setEnabled(hasSelection);
    if (copyAction) copyAction->setEnabled(hasSelection);
    updatePasteState();
}

void Editor::updatePasteState() {
    if (pasteAction) {
        pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
    }
}

void Editor::handleUndo() {
    undo();
    updateActionStates();
}

void Editor::handleCut() {
    cut();
    updateActionStates();
}

void Editor::handleCopy() {
    copy();
    updateActionStates();
}

void Editor::handlePaste() {
    paste();
    updateActionStates();
}

void Editor::handleFind() {
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("查找"),
                                               tr("输入要查找的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || searchText.isEmpty())
        return;

    QTextDocument::FindFlags flags;
    // 如果需要区分大小写，可以加一个checkbox来决定
    // flags |= QTextDocument::FindCaseSensitively;

    QTextCursor cursor = textCursor();
    cursor = document()->find(searchText, cursor, flags);

    if (!cursor.isNull()) {
        setTextCursor(cursor);
    } else {
        // 没找到就从头开始找
        cursor = document()->find(searchText, QTextCursor(document()));
        if (!cursor.isNull()) {
            setTextCursor(cursor);
        } else {
            qDebug() << "未找到匹配的文本: " << searchText;
        }
    }
}

void Editor::handleReplace() {
    bool ok;
    QString searchText = QInputDialog::getText(this, tr("替换"),
                                               tr("输入要查找的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || searchText.isEmpty())
        return;

    QString replaceText = QInputDialog::getText(this, tr("替换"),
                                                tr("替换为:"), QLineEdit::Normal,
                                                "", &ok);
    if (!ok)
        return;

    // 提供选择：替换一次还是全部替换
    QStringList options;
    options << tr("替换当前匹配") << tr("替换全部匹配");
    QString choice = QInputDialog::getItem(this, tr("替换选项"),
                                           tr("选择操作:"), options, 0, false, &ok);
    if (!ok)
        return;

    if (choice == options[0]) {
        replaceCurrent(searchText, replaceText);
    } else {
        replaceAll(searchText, replaceText);
    }
}

void Editor::replaceCurrent(const QString &searchText, const QString &replaceText) {
    QTextCursor cursor = document()->find(searchText, textCursor());
    if (!cursor.isNull()) {
        cursor.insertText(replaceText);
        setTextCursor(cursor);
    } else {
        qDebug() << "未找到匹配的文本: " << searchText;
    }
}

void Editor::replaceAll(const QString &searchText, const QString &replaceText) {
    QTextCursor cursor(document());
    int count = 0;
    while (!(cursor = document()->find(searchText, cursor)).isNull()) {
        cursor.insertText(replaceText);
        ++count;
    }
    qDebug() << "共替换" << count << "处匹配文本";
}

void Editor::handleInsert() {
    bool ok;
    QString insertText = QInputDialog::getText(this, tr("插入文本"),
                                               tr("输入要插入的内容:"), QLineEdit::Normal,
                                               "", &ok);
    if (!ok || insertText.isEmpty())
        return;

    QTextCursor cursor = textCursor();
    cursor.insertText(insertText);
    setTextCursor(cursor);
}
