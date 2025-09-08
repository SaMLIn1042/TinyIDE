#include "compiler.h"
#include <QTemporaryFile>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

QString program = "D:/Program Files/MinGW/bin/gcc.exe";

Compiler::Compiler(QObject *parent) : QObject(parent), m_process(new QProcess(this)) {
    // 连接QProcess的信号到我们的槽函数
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onProcessFinished);
    connect(m_runProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Compiler::onRunProcessFinished);
}

// compiler.cpp
void Compiler::compile(const QString &sourceCode) {
    // 清空之前的输出
    m_process->close();

    // 确保临时目录存在
    QDir tempDir(QDir::tempPath());
    if (!tempDir.exists()) {
        emit compileFinished(false, "错误：临时目录不存在");
        return;
    }

    // 创建临时文件路径
    QString tempFilePath = tempDir.absoluteFilePath("TinyIDE_temp.c");
    qDebug() << "临时文件路径:" << tempFilePath;

    // 删除可能已存在的文件
    if (QFile::exists(tempFilePath)) {
        qDebug() << "删除已存在的临时文件";
        QFile::remove(tempFilePath);
    }

    // 创建并写入文件
    QFile file(tempFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit compileFinished(false, "错误：无法创建临时文件: " + tempFilePath);
        return;
    }

    QTextStream out(&file);
    out << sourceCode;
    file.close();

    // 验证文件是否真的创建成功
    if (!QFile::exists(tempFilePath)) {
        emit compileFinished(false, "错误：临时文件创建失败: " + tempFilePath);
        return;
    }

    // 设置可执行文件路径 - 放在临时目录中
    m_executablePath = QDir::currentPath() + "/TinyIDE_output.exe";
    qDebug() << "可执行文件路径:" << m_executablePath;

    // 删除可能已存在的可执行文件
    if (QFile::exists(m_executablePath)) {
        QFile::remove(m_executablePath);
    }

    // 设置编译命令
    QString program = "gcc";
    QStringList arguments;
    arguments << "-static" << "-o" << m_executablePath << tempFilePath;

    qDebug() << "执行命令:" << program << arguments;

    // 设置工作目录为临时目录
    m_process->setWorkingDirectory(tempDir.absolutePath());

    // 启动编译过程
    m_process->start(program, arguments);

    if (!m_process->waitForStarted(5000)) {
        emit compileFinished(false, "错误：无法启动编译器进程。请确保GCC已安装并在PATH中。");
        return;
    }

    // 等待进程完成
    if (!m_process->waitForFinished(15000)) {
        emit compileFinished(false, "错误：编译过程超时");
        return;
    }
}

void Compiler::runProgram() {
    // 检查当前是否已有进程在运行
    if (m_runProcess->state() != QProcess::NotRunning) {
        qDebug() << "已有进程在运行，先终止它";
        m_runProcess->kill();
        m_runProcess->waitForFinished(1000);
    }

    // 重置进程状态
    m_runProcess->close();

    qDebug() << "尝试运行程序:" << m_executablePath;

    // 检查可执行文件是否存在
    if (m_executablePath.isEmpty() || !QFile::exists(m_executablePath)) {
        QString error = "错误：没有可执行文件或文件不存在\n";
        error += "路径: " + m_executablePath + "\n";
        error += "文件存在: " + QString(QFile::exists(m_executablePath) ? "是" : "否");
        qDebug() << error;
        emit runFinished(false, error);
        return;
    }

    qDebug() << "运行程序:" << m_executablePath;

    // 设置工作目录为可执行文件所在目录
    QFileInfo exeInfo(m_executablePath);
    m_runProcess->setWorkingDirectory(exeInfo.path());

    // 启动程序
    m_runProcess->start(m_executablePath, QStringList());

    if (!m_runProcess->waitForStarted(5000)) {
        QString error = "错误：无法启动程序\n";
        error += "错误详情: " + m_runProcess->errorString();
        qDebug() << error;
        emit runFinished(false, error);
        return;
    }

    qDebug() << "程序已启动，进程ID:" << m_runProcess->processId();

    // 等待程序完成
    if (!m_runProcess->waitForFinished(10000)) {
        qDebug() << "程序运行超时，尝试终止";
        m_runProcess->kill();
        m_runProcess->waitForFinished(1000);
        emit runFinished(false, "错误：程序运行超时");
        return;
    }

    // 获取程序输出
    QString standardOutput = QString::fromLocal8Bit(m_runProcess->readAllStandardOutput());
    QString standardError = QString::fromLocal8Bit(m_runProcess->readAllStandardError());

    qDebug() << "程序退出代码:" << m_runProcess->exitCode();
    qDebug() << "程序输出:" << standardOutput;
    qDebug() << "程序错误:" << standardError;

    // 发送运行完成信号
    QString fullOutput;
    if (!standardOutput.isEmpty()) {
        fullOutput += "程序输出:\n" + standardOutput + "\n\n";
    }
    if (!standardError.isEmpty()) {
        fullOutput += "程序错误:\n" + standardError + "\n\n";
    }

    fullOutput += "程序退出代码: " + QString::number(m_runProcess->exitCode());

    emit runFinished(m_runProcess->exitCode() == 0, fullOutput);
}

void Compiler::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString standardOutput = m_process->readAllStandardOutput();
    QString standardError = m_process->readAllStandardError();

    QString fullOutput;
    if (!standardOutput.isEmpty()) {
        fullOutput += "编译输出:\n" + standardOutput + "\n\n";
    }
    if (!standardError.isEmpty()) {
        fullOutput += "编译错误:\n" + standardError + "\n\n";
    }

    fullOutput += "退出代码: " + QString::number(exitCode);

    // 检查可执行文件是否已创建
    if (exitCode == 0) {
        if (QFile::exists(m_executablePath)) {
            qDebug() << "可执行文件已创建:" << m_executablePath;
            qDebug() << "文件大小:" << QFileInfo(m_executablePath).size() << "字节";
            fullOutput += "\n可执行文件: " + m_executablePath;
        } else {
            qDebug() << "错误：可执行文件未创建";
            fullOutput += "\n错误：可执行文件未创建";
            exitCode = 1; // 标记为失败
        }
    }

    emit compileFinished(exitCode == 0, fullOutput);
}

void Compiler::onRunProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString standardOutput = m_runProcess->readAllStandardOutput();
    QString standardError = m_runProcess->readAllStandardError();

    QString fullOutput;
    if (!standardOutput.isEmpty()) {
        fullOutput += "程序输出:\n" + standardOutput + "\n\n";
    }
    if (!standardError.isEmpty()) {
        fullOutput += "程序错误:\n" + standardError + "\n\n";
    }

    fullOutput += "程序退出代码: " + QString::number(exitCode);

    emit runFinished(exitCode == 0, fullOutput);
}
