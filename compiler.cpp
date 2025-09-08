#include "compiler.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <QCoreApplication>

Compiler::Compiler(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this)),
      m_runProcess(new QProcess(this)),
      m_compileSuccess(false)
{
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onProcessFinished);
    connect(m_runProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onRunProcessFinished);
}

Compiler::~Compiler() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished();
    }
    if (m_runProcess->state() != QProcess::NotRunning) {
        m_runProcess->kill();
        m_runProcess->waitForFinished();
    }
}

void Compiler::compile(const QString &sourceCode) {
    m_compileSuccess = false;
    m_process->close();

    // 创建临时目录确保安全
    QDir tempDir(QDir::tempPath());
    if (!tempDir.exists()) {
        emit compileFinished(false, "错误：临时目录不存在: " + tempDir.path());
        return;
    }

    // 修复：使用QString格式化文件名
    QString tempFileName = QString("TinyIDE_%1_%2.c")
                          .arg(QCoreApplication::applicationPid())
                          .arg(qrand());
    QString tempFilePath = tempDir.absoluteFilePath(tempFileName);

    // 删除可能已存在的文件
    if (QFile::exists(tempFilePath)) {
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

    // 验证文件是否创建成功
    if (!QFile::exists(tempFilePath)) {
        emit compileFinished(false, "错误：临时文件创建失败: " + tempFilePath);
        return;
    }

    // 修复：使用QString格式化可执行文件名
    QString execFileName = QString("TinyIDE_output_%1.exe")
                          .arg(QCoreApplication::applicationPid());
    m_executablePath = tempDir.absoluteFilePath(execFileName);

    // 删除可能已存在的可执行文件
    if (QFile::exists(m_executablePath)) {
        QFile::remove(m_executablePath);
    }

    // 设置编译命令 - 使用绝对路径
    QStringList arguments;
    arguments << "-o" << m_executablePath << tempFilePath << "-static";

    qDebug() << "编译命令: gcc" << arguments;

    // 设置工作目录为临时目录
    m_process->setWorkingDirectory(tempDir.absolutePath());

    // 启动编译过程
    m_process->start("gcc", arguments);

    if (!m_process->waitForStarted(3000)) {
        QString error = "错误：无法启动编译器\n";
        error += "请确保GCC已安装并在PATH中\n";
        error += "尝试的命令: gcc " + arguments.join(" ");
        emit compileFinished(false, error);
        return;
    }

    // 保存临时文件路径供后续使用
    m_tempFilePath = tempFilePath;
}

void Compiler::runProgram() {
    if (!m_compileSuccess || !QFile::exists(m_executablePath)) {
        emit runOutput("错误：请先成功编译程序");
        emit runFinished(false, "未找到可执行文件");
        return;
    }

    // 清理之前的运行状态
    if (m_runProcess->state() != QProcess::NotRunning) {
        m_runProcess->kill();
        m_runProcess->waitForFinished();
    }

    // 确保每次运行都重新连接信号
    disconnect(m_runProcess, &QProcess::readyReadStandardOutput, 0, 0);
    disconnect(m_runProcess, &QProcess::readyReadStandardError, 0, 0);

    // 连接实时输出
    connect(m_runProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QString output = QString::fromLocal8Bit(m_runProcess->readAllStandardOutput());
        emit runOutput(output);
    });

    connect(m_runProcess, &QProcess::readyReadStandardError, this, [this]() {
        QString error = QString::fromLocal8Bit(m_runProcess->readAllStandardError());
        emit runOutput("[ERROR] " + error);
    });

    // 启动程序
    QFileInfo exeInfo(m_executablePath);
    m_runProcess->setWorkingDirectory(exeInfo.path());

    // 设置合并输出通道，确保能捕获所有输出
    m_runProcess->setProcessChannelMode(QProcess::MergedChannels);

    // 启动程序
    m_runProcess->start(m_executablePath);

    if (!m_runProcess->waitForStarted(1000)) {
        emit runOutput("启动失败: " + m_runProcess->errorString());
        return;
    }

    // 等待程序完成，但设置超时时间
    if (!m_runProcess->waitForFinished(5000)) { // 5秒超时
        emit runOutput("程序运行超时，可能正在等待输入...");
        // 尝试发送回车键
        m_runProcess->write("\n");
        if (!m_runProcess->waitForFinished(2000)) {
            m_runProcess->kill();
            emit runOutput("已终止程序");
        }
    }
}

void Compiler::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    QString output = m_process->readAllStandardOutput() +
                     m_process->readAllStandardError();

    m_compileSuccess = (exitCode == 0 && QFile::exists(m_executablePath));

    QString result = QString("编译%1！\n退出代码: %2\n%3")
                    .arg(m_compileSuccess ? "成功" : "失败")
                    .arg(exitCode)
                    .arg(output);

    // 删除临时源文件
    if (!m_tempFilePath.isEmpty() && QFile::exists(m_tempFilePath)) {
        QFile::remove(m_tempFilePath);
    }

    emit compileFinished(m_compileSuccess, result);
}

void Compiler::onRunProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus)

    // 获取所有输出
    QString output = QString::fromLocal8Bit(m_runProcess->readAllStandardOutput());
    QString error = QString::fromLocal8Bit(m_runProcess->readAllStandardError());

    QString result = QString("程序运行结束\n退出代码: %1\n").arg(exitCode);
    if (!output.isEmpty()) {
        result += "输出:\n" + output;
    }
    if (!error.isEmpty()) {
        result += "错误:\n" + error;
    }

    emit runFinished(exitCode == 0, result);
}
