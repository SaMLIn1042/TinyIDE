#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QToolBar>
#include <QSplitter>
#include <QListWidget>
#include <QTextEdit>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNewFileOrProject();
    void onOpenFileOrProject();
    void onCloseClicked();

private:
    Ui::MainWindow *ui;
    QPushButton* closeButton;
    QToolBar *leftToolBar;
    QListWidget *fileListWidget;
    QTextEdit *codeEditor;
};

#endif // MAINWINDOW_H
