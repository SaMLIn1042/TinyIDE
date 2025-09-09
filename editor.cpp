#include "editor.h"

Editor::Editor(QWidget *parent) : QPlainTextEdit(parent) {}

QString Editor::getCodeText() const
{
    // 实现：直接返回当前文本即可
    return toPlainText();
}
