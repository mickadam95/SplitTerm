#ifndef TERMINALBACKEND_H
#define TERMINALBACKEND_H

#include <QObject>
#include <QSocketNotifier>
#include <sys/types.h>
#include <termios.h>

class TerminalBackend : public QObject
{
    Q_OBJECT
public:
    explicit TerminalBackend(QObject *parent = nullptr);
    ~TerminalBackend();

    void startShell(const QString &shellPath);
    void sendCommand(const QString &command);
    void requestCwdUpdate(); // New method to request CWD via OSC 7
    QString getCwdFromProc() const; // New method for /proc fallback

signals:
    void readyReadOutput(const QByteArray &data);
    void pwdOutput(const QString &dir);
    void shellExited();

private slots:
    void handlePtyOutput();

private:
    int masterFd = -1;
    pid_t childPid = -1;
    QSocketNotifier *notifier = nullptr;
    QByteArray outputBuffer; // Buffer to handle multi-read output and parse OSC 7

    void processOutputChunk(const QByteArray &data);

    // OSC 7 Sequence: \033]7;file://HOSTNAME/PATH\007
    const QByteArray OSC7_START = "\033]7;file://";
    const QByteArray OSC7_END = "\007"; // Bell character
};

#endif // TERMINALBACKEND_H
