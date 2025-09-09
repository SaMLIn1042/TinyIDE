#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>

class Editor: public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit Editor(QWidget *parent = nullptr);

    // 添加一个公共方法，用于获取文本
    QString getCodeText() const;

signals:
};

#endif // EDITOR_H
