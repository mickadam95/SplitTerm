#ifndef TERMINALBACKEND_H
#define TERMINALBACKEND_H

#include <QObject>
#include <QSocketNotifier>
#include <sys/types.h>
#include <termios.h>
#include <QColor>
#include <QHash>

class TerminalBackend : public QObject
{
    Q_OBJECT
public:
    explicit TerminalBackend(QObject *parent = nullptr);
    ~TerminalBackend();

    void startShell(const QString &shellPath);
    void sendCommand(const QString &command);
    QString getCwdFromProc() const;

public slots:
    // Slot to be called when settings change
    void loadColorSettings();

signals:
    // This replaces readyReadOutput
    void readyReadHtml(const QString &html);
    void pwdOutput(const QString &dir);
    void shellExited();

private slots:
    void handlePtyOutput();

private:
    int masterFd = -1;
    pid_t childPid = -1;
    QSocketNotifier *notifier = nullptr;
    QByteArray outputBuffer;

    void processOutputChunk(const QByteArray &data);

    // OSC 7 Sequence
    const QByteArray OSC7_START = "\033]7;file://";
    const QByteArray OSC7_END = "\007";

    // --- New ANSI Parsing Members ---

    // Tracks the current style
    QColor currentFgColor;
    bool currentBold = false;

    // Map of ANSI codes to colors (loaded from QSettings)
    QHash<int, QColor> ansiColorMap;

    // Resets style to default
    void resetSgrState();

    // Parses SGR codes (e.g., "[31;1m") and returns HTML style string
    QString parseSgrCodes(const QStringList &codes);

    // Helper to generate the current style span
    QString getCurrentStyleHtml() const;
};

#endif // TERMINALBACKEND_H
