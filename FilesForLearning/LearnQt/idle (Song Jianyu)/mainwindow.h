#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QMenu>
#include <QMenuBar>
#include <QAction>

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    QTextEdit *text1;
    QMenu *file;
    QMenu *edit;
    QMenu *help;

    QAction *file_open;
    QAction *file_exit;


public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
};
#endif // MAINWINDOW_H
