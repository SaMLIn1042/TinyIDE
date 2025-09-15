#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "editor.h"
#include "compiler.h"
#include <QString>
#include <QMessageBox>
#include <QListWidget>

namespace Ui
{
    class MainWindow;
}

// 文件标签信息结构体
struct FileTabInfo {
    Editor* editor;
    QString filePath;
    bool isSaved;
    QString displayName; // 显示在标签上的名称
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    Editor *m_editor;
    Compiler *m_compiler;
    QString m_currentFilePath;
    QWidget *m_inputWidget;
    QTabWidget *m_tabWidget;
    QLineEdit *m_inputLineEdit;
    bool m_isSaved;
    // 存储标签页信息
    QVector<FileTabInfo> m_tabInfos;
    int m_currentTabIndex;
    Editor* currentEditor() const;

private slots:
    void on_actionCompile_triggered();
    void on_actionRun_triggered();
    void onCompileFinished(bool success, const QString &output);
    void onRunFinished(bool success, const QString &output);
    void handleRunOutput(const QString &output);
    void on_actionNew_triggered();
    void on_actionOpen_triggered();
    bool on_actionSave_triggered();
    bool on_actionSaveAs_triggered();
    void on_actionClose_triggered();
    void on_actionExit_triggered();
    void on_actionStop_triggered();
    void onEditorTextChanged();
    void onSendInput();
    // 新增的槽函数
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void updateTabTitle(int index);
};

#endif // MAINWINDOW_H
