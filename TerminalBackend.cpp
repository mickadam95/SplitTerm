#include "TerminalBackend.h"

TerminalBackend::TerminalBackend(QObject* parent)
    :QObject(parent), process(new QProcess(this)){

    connect(process, &QProcess::readyReadStandardOutput, this, &TerminalBackend::handleStdOut);
    connect(process, &QProcess::readyReadStandardError, this, &TerminalBackend::handleStdErr);

    TerminalBackend::~TerminalBackend() {
        if (process->state() != QProcess::NotRunning) {
            process->terminate();
            process->waitForFinished(1000);
        }
    }

}
