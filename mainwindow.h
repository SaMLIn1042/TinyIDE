#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollBar>
#include "editor.h"
#include "compiler.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void clearHighlights();  // 清空高亮
    void highlightSelection(); //高亮
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    Editor *m_editor;       // 编辑器实例
    Compiler *m_compiler;   // 编译器实例
    QString m_lastFindText;  // 保存最近一次查找关键字
    void highlightAllMatches(const QString &text);
    void findNext();
    void findPrevious();//用于实现查找

private slots:
    // 原有功能槽函数（编译、运行、回调）
    void on_actionCompile_triggered();
    void on_actionRun_triggered();
    void onCompileFinished(bool success, const QString &output);
    void onRunFinished(bool success, const QString &output);
    void handleRunOutput(const QString &output);

    // 新增：编辑功能槽函数声明（解决“未声明”错误）
    void on_actionUndo_triggered();        // 撤销
    void on_actionRedo_triggered();        // 恢复
    void on_actionCut_triggered();         // 剪切
    void on_actionCopy_triggered();        // 复制
    void on_actionPaste_triggered();       // 粘贴
    void on_actionFind_triggered();        // 查找
    void on_actionReplace_triggered();     // 替换
    void on_actionInsert_triggered();      // 插入
};

#endif // MAINWINDOW_H
