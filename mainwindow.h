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
    QString m_currentFilePath;
    QListWidget *fileListWidget;
    QMap<QString, QString> fileMap;
    QWidget *m_inputWidget;
    QLineEdit *m_inputLineEdit;
    bool m_isSaved;

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
    void onFileItemClicked(QListWidgetItem *item);
    void updateFileList();
    void onSendInput();
};

#endif // MAINWINDOW_H
