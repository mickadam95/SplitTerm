#include "terminalbackend.h"
#include <QDebug>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <QRegularExpression>

TerminalBackend::TerminalBackend(QObject *parent) : QObject(parent) {}

TerminalBackend::~TerminalBackend() {
    if (childPid > 0) {
        kill(childPid, SIGTERM);
        waitpid(childPid, nullptr, 0);
    }
    if (masterFd >= 0) close(masterFd);
    if (notifier) notifier->deleteLater();
}

void TerminalBackend::startShell(const QString &shellPath) {
    struct winsize ws{};
    ws.ws_col = 80;
    ws.ws_row = 24;

    childPid = forkpty(&masterFd, nullptr, nullptr, &ws);
    if (childPid < 0) {
        qWarning() << "forkpty failed";
        return;
    }

    if (childPid == 0) {
        const char *bash = shellPath.toUtf8().constData();
        execl(bash, bash, "--noprofile", "--norc", "-i", nullptr);
        _exit(1);
    } else {
        notifier = new QSocketNotifier(masterFd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated,
                this, &TerminalBackend::handlePtyOutput);
    }
}

void TerminalBackend::sendCommand(const QString &command) {
    if (masterFd < 0) return;
    silentMode = false;
    QByteArray data = command.toUtf8() + "\n";
    write(masterFd, data.constData(), data.size());
}

void TerminalBackend::sendSilentCommand(const QString &command) {
    if (masterFd < 0) return;
    silentMode = true;
    QByteArray data = command.toUtf8() + "\n";
    write(masterFd, data.constData(), data.size());
}

void TerminalBackend::handlePtyOutput() {
    if (masterFd < 0) return;

    char buffer[4096];
    ssize_t n = read(masterFd, buffer, sizeof(buffer));
    if (n <= 0) return;

    QByteArray data(buffer, static_cast<int>(n));

    if (silentMode) {
        handleSilentOutput(data);
        return;
    }

    // Normal output: forward directly
    emit readyReadOutput(data);
}

void TerminalBackend::handleSilentOutput(QByteArray data) {
    QString text = QString::fromUtf8(data);

    // Remove ANSI escape sequences
    QRegularExpression re("\x1B\\[[0-?]*[ -/]*[@-~]");
    text.remove(re);

    // Split lines and find first line starting with '/'
    QStringList lines = text.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("/")) {
            emit pwdOutput(trimmed);
            break;
        }
    }

    silentMode = false;
}
