#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    Editor *m_editor;  // 使用UI中已有的编辑器
    Compiler *m_compiler;

private slots:
    void on_actionCompile_triggered();
    void on_actionRun_triggered();
    void onCompileFinished(bool success, const QString &output);
    void onRunFinished(bool success, const QString &output);
    void handleRunOutput(const QString &output);
};

#endif // MAINWINDOW_H
