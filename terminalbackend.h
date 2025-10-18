#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QByteArray>

class TerminalBackend : public QObject
{
    Q_OBJECT
public:
    explicit TerminalBackend(QObject *parent = nullptr);
    ~TerminalBackend();

    void startShell(const QString &shellPath = "/bin/bash");
    void sendCommand(const QString &command);

    // Silent command: output is only used internally (placeholder)
    void sendSilentCommand(const QString &command);

signals:
    void readyReadOutput(const QByteArray &data);
    void pwdOutput(const QString &cwd);

private slots:
    void handlePtyOutput();

private:
    void handleSilentOutput(QByteArray data);

    int masterFd = -1;
    pid_t childPid = -1;
    QSocketNotifier *notifier = nullptr;

    bool silentMode = false;
};
