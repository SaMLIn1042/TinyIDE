#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "editor.h"
#include "compiler.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    Editor *m_editor;
    Compiler *m_compiler;

private slots:
    void on_actionCompile_triggered();
    //声明一个槽函数来处理编译完成信号
    void onCompileFinished(bool success, const QString &output);
};

#endif // MAINWINDOW_H
