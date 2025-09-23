
#include "compiler.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

// 编译器构造函数，初始化进程和状态
Compiler::Compiler(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this)),    // 编译进程
      m_runProcess(new QProcess(this)), // 运行进程
      m_compileSuccess(false)           // 初始编译状态
{
    // 连接编译进程完成信号
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onProcessFinished);

    // 连接运行进程完成信号
    connect(m_runProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Compiler::onRunProcessFinished);
}

// 编译器析构函数，确保进程终止
Compiler::~Compiler()
{
    // 确保所有进程在析构时终止
    if (m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished();
    }
    if (m_runProcess->state() != QProcess::NotRunning)
    {
        m_runProcess->kill();
        m_runProcess->waitForFinished();
    }
}

// 编译源代码：预处理、保存到临时文件、调用GCC编译
void Compiler::compile(const QString &sourceCode)
{
    m_compileSuccess = false; // 重置编译状态
    m_process->close();       // 关闭之前的编译进程

    // 预处理源代码：添加必要头文件和行缓冲设置
    QString modifiedCode = sourceCode;

    // 确保包含必要头文件
    if (!modifiedCode.contains("#include <stdio.h>"))
    {
        modifiedCode.prepend("#include <stdio.h>\n");
    }
    if (!modifiedCode.contains("#include <stdlib.h>"))
    {
        modifiedCode.prepend("#include <stdlib.h>\n");
    }

    // 在main()函数开头插入行缓冲设置
    QRegularExpression mainRegex(R"(int\s+main\s*\([^)]*\)\s*\{)");
    QRegularExpressionMatch match = mainRegex.match(modifiedCode);

    if (match.hasMatch())
    {
        int insertPos = match.capturedEnd();
        QString insertion = "\n    setvbuf(stdout, NULL, _IONBF, 0); // IDE: 启用行缓冲\n";
        modifiedCode.insert(insertPos, insertion);
    }
    else
    {
        // 处理void main()等非标准形式
        mainRegex.setPattern(R"(void\s+main\s*\([^)]*\)\s*\{)");
        match = mainRegex.match(modifiedCode);
        if (match.hasMatch())
        {
            int insertPos = match.capturedEnd();
            QString insertion = "\n    setvbuf(stdout, NULL, _IONBF, 0); // IDE: 启用行缓冲\n";
            modifiedCode.insert(insertPos, insertion);
        }
    }

    // 检查临时目录
    QDir tempDir(QDir::tempPath());
    if (!tempDir.exists())
    {
        emit compileFinished(false, "错误：临时目录不存在: " + tempDir.path());
        return;
    }

    // 生成唯一临时文件名
    QString tempFileName = QString("TinyIDE_%1_%2.c")
                               .arg(QCoreApplication::applicationPid())
                               .arg(qrand());
    QString tempFilePath = tempDir.absoluteFilePath(tempFileName);

    // 清理可能存在的旧文件
    if (QFile::exists(tempFilePath))
    {
        QFile::remove(tempFilePath);
    }

    // 创建并写入临时源文件
    QFile file(tempFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        emit compileFinished(false, "错误：无法创建临时文件: " + tempFilePath);
        return;
    }

    // 写入修改后的代码
    QTextStream out(&file);
    out << modifiedCode;
    file.close();

    // 验证文件创建
    if (!QFile::exists(tempFilePath))
    {
        emit compileFinished(false, "错误：临时文件创建失败: " + tempFilePath);
        return;
    }

    // 生成可执行文件名
    QString execFileName = QString("TinyIDE_output_%1.exe")
                               .arg(QCoreApplication::applicationPid());
    m_executablePath = tempDir.absoluteFilePath(execFileName);

    // 清理可能存在的旧可执行文件
    if (QFile::exists(m_executablePath))
    {
        QFile::remove(m_executablePath);
    }

    // 设置编译参数
    QStringList arguments;
    arguments << "-o" << m_executablePath << tempFilePath << "-static";
    qDebug() << "编译命令: gcc" << arguments;

    // 设置工作目录
    m_process->setWorkingDirectory(tempDir.absolutePath());

    // 启动编译进程
    m_process->start("gcc", arguments);

    // 检查进程启动
    if (!m_process->waitForStarted(3000))
    {
        QString error = "错误：无法启动编译器\n";
        error += "请确保GCC已安装并在PATH中\n";
        error += "尝试的命令: gcc " + arguments.join(" ");
        emit compileFinished(false, error);
        return;
    }

    // 保存临时文件路径供后续清理
    m_tempFilePath = tempFilePath;
}

// 运行编译成功的程序
void Compiler::runProgram()
{
    // 检查编译状态和可执行文件
    if (!m_compileSuccess)
    {
        emit runOutput("错误：请先成功编译程序");
        emit runFinished(false, "编译未成功");
        return;
    }

    // 添加额外的可执行文件存在检查
    if (!QFile::exists(m_executablePath))
    {
        emit runOutput("错误：可执行文件不存在，请重新编译");
        emit runFinished(false, "可执行文件不存在");
        m_compileSuccess = false; // 重置编译状态
        return;
    }

    // 清理之前的运行进程
    if (m_runProcess->state() != QProcess::NotRunning)
    {
        m_runProcess->kill();
        m_runProcess->waitForFinished();
    }

    // 重新连接输出信号
    disconnect(m_runProcess, &QProcess::readyReadStandardOutput, 0, 0);
    disconnect(m_runProcess, &QProcess::readyReadStandardError, 0, 0);

    // 连接标准输出
    connect(m_runProcess, &QProcess::readyReadStandardOutput, this, [this]()
            {
        QString output = QString::fromLocal8Bit(m_runProcess->readAllStandardOutput());
        emit runOutput(output); });

    // 连接错误输出
    connect(m_runProcess, &QProcess::readyReadStandardError, this, [this]()
            {
        QString error = QString::fromLocal8Bit(m_runProcess->readAllStandardError());
        emit runOutput("[ERROR] " + error); });

    // 设置工作目录
    QFileInfo exeInfo(m_executablePath);
    m_runProcess->setWorkingDirectory(exeInfo.path());

    // 合并输出通道
    m_runProcess->setProcessChannelMode(QProcess::MergedChannels);

    // 启动程序
    m_runProcess->start(m_executablePath);

    // 检查启动状态
    if (!m_runProcess->waitForStarted(1000))
    {
        emit runOutput("启动失败: " + m_runProcess->errorString());
        return;
    }

    emit runStarted();
}

// 发送输入到运行中的程序
void Compiler::sendInput(const QString &input)
{
    if (m_runProcess && m_runProcess->state() == QProcess::Running)
    {
        m_runProcess->write(input.toLocal8Bit());
        m_runProcess->write("\n"); // 添加换行符
    }
}

// 停止运行中的程序
void Compiler::stopProgram()
{
    // 检查运行状态
    if (m_runProcess->state() != QProcess::NotRunning)
    {
        // 终止运行进程
        m_runProcess->kill();
        m_runProcess->waitForFinished();

        // 发送终止信号
        emit runFinished(false, "程序已被用户终止");
    }
}

// 编译进程完成处理：检查结果、清理临时文件、发送信号
void Compiler::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    // 获取编译输出
    QString output = m_process->readAllStandardOutput() +
                     m_process->readAllStandardError();

    // 检查编译结果
    m_compileSuccess = (exitCode == 0 && QFile::exists(m_executablePath));

    // 生成结果消息
    QString result = QString("编译%1！\n退出代码: %2\n%3")
                         .arg(m_compileSuccess ? "成功" : "失败")
                         .arg(exitCode)
                         .arg(output);

    // 清理临时源文件
    if (!m_tempFilePath.isEmpty() && QFile::exists(m_tempFilePath))
    {
        QFile::remove(m_tempFilePath);
    }

    // 发送编译完成信号
    emit compileFinished(m_compileSuccess, result);
}

// 运行进程完成处理：获取输出、清理可执行文件、发送信号
void Compiler::onRunProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus) // 未使用参数

    // 获取程序输出
    QString output = QString::fromLocal8Bit(m_runProcess->readAllStandardOutput());
    QString error = QString::fromLocal8Bit(m_runProcess->readAllStandardError());

    // 生成结果消息
    QString result = QString("程序运行结束\n退出代码: %1\n").arg(exitCode);
    if (!output.isEmpty())
    {
        result += "输出:\n" + output;
    }
    if (!error.isEmpty())
    {
        result += "错误:\n" + error;
    }

    // 清理可执行文件
    if (!m_executablePath.isEmpty() && QFile::exists(m_executablePath))
    {
        QFile::remove(m_executablePath);
    }

    // 发送运行完成信号
    emit runFinished(exitCode == 0, result);
}

