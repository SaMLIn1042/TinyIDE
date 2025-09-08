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
    bool m_compileSuccess;
};

#endif // COMPILER_H
