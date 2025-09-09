#ifndef COMPILER_H
#define COMPILER_H

#include <QObject>
#include <QProcess>
#include <QTemporaryFile>

class Compiler : public QObject
{
    Q_OBJECT
public:
    explicit Compiler(QObject *parent = nullptr);
    ~Compiler();

    void compile(const QString &sourceCode);
    void runProgram();

    // -------------------------- 新增：公开的状态获取函数 --------------------------
    bool isCompileSuccess() const {
        return m_compileSuccess;  // 返回私有变量m_compileSuccess的值
    }

signals:
    void compileFinished(bool success, const QString &output);
    void runFinished(bool success, const QString &output);
    void runOutput(const QString &output);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onRunProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;
    QProcess *m_runProcess;
    QString m_executablePath;
    QString m_tempFilePath;
    bool m_compileSuccess;  // 原有私有变量（无需修改）
};

#endif // COMPILER_H
