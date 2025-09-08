#include "compiler.h"
#include <QTemporaryFile>
#include <QProcess>

Compiler::Compiler(QObject *parent) : QObject(parent), m_process(new QProcess(this)) {
    // 连接QProcess的信号到我们的槽函数
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onProcessFinished);
}

void Compiler::compile(const QString &sourceCode) {
    // 1. 将源代码写入临时文件
    QTemporaryFile tempFile;
    if (tempFile.open()) {
        tempFile.write(sourceCode.toUtf8());
        tempFile.close();
    }
    // 2. 构建编译命令
    QStringList arguments;
    arguments << "-o" << "output.exe" << tempFile.fileName();
    // 3. 启动编译过程
    m_process->start("gcc", arguments);
}

void Compiler::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    // 获取编译器的输出（包括错误信息）
    QString output = m_process->readAllStandardError() + m_process->readAllStandardOutput();
    // 发出我们自定义的信号
    emit compileFinished(exitCode == 0, output);
}
