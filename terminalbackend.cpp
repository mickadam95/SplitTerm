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
#include <string.h>
#include <termios.h>
#include <QStringBuilder>
#include <QSettings> // For loading colors

TerminalBackend::TerminalBackend(QObject *parent) : QObject(parent) {
    // Load colors from QSettings on startup
    loadColorSettings();
    resetSgrState();
}

TerminalBackend::~TerminalBackend() {
    if (childPid > 0) {
        kill(childPid, SIGTERM);
        waitpid(childPid, nullptr, 0);
    }
    if (masterFd >= 0) close(masterFd);
    if (notifier) notifier->deleteLater();
}

void TerminalBackend::loadColorSettings()
{
    // Make sure QCoreApplication::setOrganizationName/setApplicationName was called in main.cpp
    QSettings settings;
    ansiColorMap.clear();

    // Loop through all 16 colors (normal and bright)
    for (int i = 30; i <= 37; ++i) {
        ansiColorMap.insert(i, settings.value(QString("ansi/%1").arg(i)).value<QColor>());
    }
    for (int i = 90; i <= 97; ++i) {
        ansiColorMap.insert(i, settings.value(QString("ansi/%1").arg(i)).value<QColor>());
    }

    qDebug() << "Loaded" << ansiColorMap.size() << "colors from settings.";
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
        // --- FIX 1: Revert to simple execl ---
        // This avoids the "line ending" warning from the shell.
        const char *bash = shellPath.toUtf8().constData();
        execl(bash, bash, "--noprofile", "--norc", "-i", nullptr);
        _exit(1);
    } else {
        // Parent process:
        notifier = new QSocketNotifier(masterFd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated,
                this, &TerminalBackend::handlePtyOutput);

        // --- FIX 1 (cont.): Send setup commands via write() ---
        // Now that `termios` handles echo, this is the clean way.
        const char* bpm_cmd = "bind 'set enable-bracketed-paste off'\n";
        write(masterFd, bpm_cmd, strlen(bpm_cmd));

        const char* ps1_cmd = "export PS1=''\n";
        write(masterFd, ps1_cmd, strlen(ps1_cmd));

        const char* ps2_cmd = "export PS2=''\n";
        write(masterFd, ps2_cmd, strlen(ps2_cmd));
    }
}

void TerminalBackend::sendCommand(const QString &command) {
    if (masterFd < 0) return;
    QByteArray data = command.toUtf8() + "\n";
    write(masterFd, data.constData(), data.size());
}

QString TerminalBackend::getCwdFromProc() const {
    if (childPid <= 0) return QString();

    QString procPath = QString("/proc/%1/cwd").arg(childPid);
    QFile symlinkFile(procPath);
    QString target = symlinkFile.symLinkTarget();

    return target;
}


void TerminalBackend::handlePtyOutput() {
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

void TerminalBackend::resetSgrState() {
    // Default terminal color is often not black,
    // so an invalid QColor means "use default".
    currentFgColor = QColor();
    currentBold = false;
}

QString TerminalBackend::getCurrentStyleHtml() const {
    if (!currentFgColor.isValid() && !currentBold) {
        return QString(); // No style
    }

    QStringList styles;
    if (currentFgColor.isValid()) {
        styles.append(QString("color:%1;").arg(currentFgColor.name()));
    }
    if (currentBold) {
        styles.append("font-weight:bold;");
    }
    return QString(" style=\"%1\"").arg(styles.join(""));
}

QString TerminalBackend::parseSgrCodes(const QStringList &codes) {
    if (codes.isEmpty()) {
        return QString();
    }

    bool changed = false;
    for (const QString &codeStr : codes) {
        int code = codeStr.toInt();
        if (code == 0) { // Reset
            resetSgrState();
            changed = true;
        } else if (code == 1) { // Bold
            currentBold = true;
            changed = true;
        } else if (code == 22) { // Normal intensity
            currentBold = false;
            changed = true;
        } else if (ansiColorMap.contains(code)) {
            // Standard & Bright Colors
            currentFgColor = ansiColorMap.value(code, QColor());
            changed = true;
        } else if (code == 39) { // Default FG color
            currentFgColor = QColor();
            changed = true;
        }
        // TODO: Add 40-47 for background colors
    }

    // If style changed, close old span and open a new one
    if (changed) {
        return QString("</span><span%1>").arg(getCurrentStyleHtml());
    }
    return QString();
}

void TerminalBackend::processOutputChunk(const QByteArray &data) {
    outputBuffer.append(data);

    // This regex finds:
    // 1. OSC 7 (file path)
    // 2. SGR codes (colors, bold) - Captures the codes
    // 3. Other CSI codes (like BPM) - Matches but does not capture
    static QRegularExpression re(
        "\x1B\\]7;.*?\x07"             // OSC 7
        "|\x1B\\[([0-9;?]*)m"          // SGR (color/style)
        "|\x1B\\[\\?[0-9;]*[a-zA-Z]"   // Other CSI (like BPM [?2004h)
        );

    QString htmlChunk;
    int lastPos = 0;

    auto it = re.globalMatch(outputBuffer);
    while (it.hasNext()) {
        auto match = it.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();

        // 1. Append any plain text *before* this match
        if (start > lastPos) {
            QString plainText = QString::fromUtf8(outputBuffer.mid(lastPos, start - lastPos));

            // --- FIX 3: Simplify HTML escaping ---
            // Only escape ampersand and newline
            plainText.replace(QLatin1String("&"), QLatin1String("&amp;"));
            plainText.replace(QLatin1String("\n"), QLatin1String("<br>"));
            plainText.replace(QLatin1String("\r"), QLatin1String("")); // Ignore carriage return

            htmlChunk.append(plainText);
        }

        // 2. Process the matched escape sequence
        QString matchedData = match.captured(0);
        if (matchedData.startsWith("\x1B]7;")) {
            // --- FIX 2: Correct OSC 7 Parsing ---
            QByteArray oscData = matchedData.toUtf8();
            int hostStart = oscData.indexOf("file://") + 7; // Find start of hostname
            if (hostStart != -1 + 7) {
                int pathStart = oscData.indexOf('/', hostStart); // Find first slash *after* hostname
                if (pathStart != -1) {
                    // Extract from that slash to the end, minus the \x07
                    QString path = QString::fromUtf8(oscData.mid(pathStart, oscData.length() - pathStart - 1));
                    emit pwdOutput(path);
                }
            }
        } else if (matchedData.endsWith('m') && match.lastCapturedIndex() == 1) {
            // It's an SGR code (and we captured group 1)
            QStringList codes = match.captured(1).split(';');
            htmlChunk.append(parseSgrCodes(codes));
        } else {
            // It's another code (like BPM) that we matched but don't process.
            // Do nothing, just skip it.
        }

        lastPos = end;
    }

    // --- FIX 2: Correct Buffer Handling ---
    // Only remove the data we have successfully processed.
    // Any remaining data (e.g., an incomplete escape code) will
    // stay in outputBuffer for the next read.
    outputBuffer.remove(0, lastPos);

    // We no longer process "remaining data" here, as it's
    // just an incomplete fragment.

    if (!htmlChunk.isEmpty()) {
        // We wrap everything in our current style span
        emit readyReadHtml(QString("<span%1>%2</span>").arg(getCurrentStyleHtml()).arg(htmlChunk));
    }
}
