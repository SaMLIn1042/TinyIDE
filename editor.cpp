#include "editor.h"
#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QClipboard>
#include <QDebug>

Editor::Editor(QWidget *parent) : QPlainTextEdit(parent),
    undoAction(nullptr), cutAction(nullptr),
    copyAction(nullptr), pasteAction(nullptr)
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
        }
    }

    qDebug() << "动作匹配情况：";
    qDebug() << "actionUndo: " << (undoAction ? "找到" : "未找到");
    qDebug() << "actionCut: " << (cutAction ? "找到" : "未找到");
    qDebug() << "actionCopy: " << (copyAction ? "找到" : "未找到");
    qDebug() << "actionPaste: " << (pasteAction ? "找到" : "未找到");
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
