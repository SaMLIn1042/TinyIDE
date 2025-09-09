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
    QString m_currentFilePath;// 新增：当前文件路径
    QListWidget *fileListWidget;
    QMap<QString, QString> fileMap; // 文件名到文件路径的映射
    bool m_isSaved; // 新增：跟踪文件是否已保存

private slots:
    void on_actionCompile_triggered();
    void on_actionRun_triggered();
    void onCompileFinished(bool success, const QString &output);
    void onRunFinished(bool success, const QString &output);
    void handleRunOutput(const QString &output);
    // 新增：文件操作槽函数
    void on_actionNew_triggered();      // 新建文件（之前已实现，这里保留）
    void on_actionOpen_triggered();     // 打开文件
    bool on_actionSave_triggered();     // 保存文件
    bool on_actionSaveAs_triggered();   // 另存为
    void on_actionClose_triggered();    //关闭
    void on_actionExit_triggered();     // 退出程序
    void onEditorTextChanged();         // 编辑器内容变化时更新状态
    void onFileItemClicked(QListWidgetItem *item);
    void updateFileList(); // 更新文件列表
};

#endif // MAINWINDOW_H
