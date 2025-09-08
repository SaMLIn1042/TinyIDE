#ifndef COMPILER_H
#define COMPILER_H

#include <QObject>
#include <QProcess>

class Compiler : public QObject
{
    Q_OBJECT
public:
    explicit Compiler(QObject *parent = nullptr);
    // 公共方法：执行编译，传入要编译的源代码
    void compile(const QString &sourceCode);

signals:
    // 自定义信号：编译完成时发出，携带是否成功和输出信息
    void compileFinished(bool success, const QString &output);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;
};

#endif // COMPILER_H
