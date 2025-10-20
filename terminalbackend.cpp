#include "terminalbackend.h"
#include <QDebug>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QFile>
#include <QHostInfo>
#include <string.h> // For strlen
#include <termios.h> // For TTY settings

TerminalBackend::TerminalBackend(QObject *parent) : QObject(parent) {}

TerminalBackend::~TerminalBackend() {
    // Full destructor
    if (childPid > 0) {
        kill(childPid, SIGTERM);
        waitpid(childPid, nullptr, 0);
    }
    if (masterFd >= 0) close(masterFd);
    if (notifier) notifier->deleteLater();
}

void TerminalBackend::startShell(const QString &shellPath) {
    // Set TTY attributes *before* forking to disable echo
    struct termios tt;
    if (tcgetattr(STDIN_FILENO, &tt) < 0) {
        qWarning() << "tcgetattr failed";
        return;
    }
    // Unset the ECHO flag. The shell will inherit this.
    tt.c_lflag &= ~ECHO;

    struct winsize ws{};
    ws.ws_col = 80;
    ws.ws_row = 24;

    // Pass the modified TTY settings (tt) to forkpty
    childPid = forkpty(&masterFd, nullptr, &tt, &ws);

    if (childPid < 0) {
        qWarning() << "forkpty failed";
        return;
    }

    if (childPid == 0) {
        // Child process: just exec the shell. It will inherit the no-echo TTY.
        const char *bash = shellPath.toUtf8().constData();
        execl(bash, bash, "--noprofile", "--norc", "-i", nullptr);
        _exit(1);
    } else {
        // Parent process:
        notifier = new QSocketNotifier(masterFd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated,
                this, &TerminalBackend::handlePtyOutput);

        // Disable Bracketed Paste Mode
        const char* bpm_cmd = "bind 'set enable-bracketed-paste off'\n";
        write(masterFd, bpm_cmd, strlen(bpm_cmd));

        // Disable the "bash-5.3$" prompt entirely
        const char* ps1_cmd = "export PS1=''\n";
        write(masterFd, ps1_cmd, strlen(ps1_cmd));
    }
}

void TerminalBackend::sendCommand(const QString &command) {
    if (masterFd < 0) return;
    QByteArray data = command.toUtf8() + "\n";
    write(masterFd, data.constData(), data.size());
}

QString TerminalBackend::getCwdFromProc() const {
    // Full function body to fix the warning
    if (childPid <= 0) return QString();

    QString procPath = QString("/proc/%1/cwd").arg(childPid);
    QFile symlinkFile(procPath);
    QString target = symlinkFile.symLinkTarget();

    return target;
}


void TerminalBackend::handlePtyOutput() {
    // Full function body
    if (masterFd < 0) return;

    char buffer[4096];
    ssize_t n = read(masterFd, buffer, sizeof(buffer));

    if (n < 0) return;

    if (n == 0) {
        emit shellExited();
        notifier->setEnabled(false);
        close(masterFd);
        masterFd = -1;
        return;
    }

    QByteArray data(buffer, static_cast<int>(n));
    processOutputChunk(data);
}

void TerminalBackend::processOutputChunk(const QByteArray &data) {
    outputBuffer.append(data);

    // 1. Check for OSC 7 sequence
    int startIndex = outputBuffer.indexOf(OSC7_START);
    int endIndex = outputBuffer.indexOf(OSC7_END, startIndex + OSC7_START.size());

    if (startIndex != -1 && endIndex != -1) {
        int startOfPath = startIndex + OSC7_START.size();
        int pathLength = endIndex - startOfPath;
        QByteArray oscData = outputBuffer.mid(startOfPath, pathLength);

        int slashIndex = oscData.indexOf('/');
        if (slashIndex != -1) {
            QString path = QString::fromUtf8(oscData.mid(slashIndex));
            emit pwdOutput(path);
        }

        outputBuffer.remove(startIndex, endIndex + OSC7_END.size() - startIndex);

        if (outputBuffer.contains(OSC7_START)) {
            processOutputChunk(QByteArray());
            return;
        }
    }

    // 2. Forward the rest of the output
    if (!outputBuffer.isEmpty()) {
        // More aggressive ANSI escape code regex
        QRegularExpression re("\x1B\\[[0-9;?]*[a-zA-Z]");
        QString text = QString::fromUtf8(outputBuffer);
        text.remove(re);

        if (!text.trimmed().isEmpty()) {
            emit readyReadOutput(text.toUtf8());
        }
        outputBuffer.clear();
    }
}
