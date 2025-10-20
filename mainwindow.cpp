#include "mainwindow.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTimer>
#include <QHostInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    outputBox = new QPlainTextEdit;
    outputBox->setReadOnly(true);
    outputBox->setPlaceholderText("Command output will appear here...");

    inputBox = new QLineEdit;
    inputBox->setPlaceholderText("Type a shell command...");
    inputBox->installEventFilter(this);

    layout->addWidget(outputBox);
    layout->addWidget(inputBox);
    central->setLayout(layout);
    setCentralWidget(central);
    resize(800, 600);

    backend = new TerminalBackend(this);
    backend->startShell("/bin/bash");

    // Request initial CWD update
    backend->sendCommand(getCwdCommand());

    // Fallback timer
    QTimer::singleShot(500, this, [this](){
        if(currentDir.isEmpty()) {
            QString procCwd = backend->getCwdFromProc();
            if (!procCwd.isEmpty()) {
                currentDir = procCwd;
                updatePrompt();
                outputBox->appendPlainText(QString("Note: CWD set via /proc fallback: %1").arg(currentDir));
            }
        }
    });

    // CONNECTS
    connect(backend, &TerminalBackend::readyReadOutput, this, [this](const QByteArray &data){
        outputBox->insertPlainText(QString::fromUtf8(data));
        outputBox->verticalScrollBar()->setValue(outputBox->verticalScrollBar()->maximum());
    });

    connect(backend, &TerminalBackend::pwdOutput, this, [this](const QString &dir){
        currentDir = dir;
        updatePrompt();
    });

    connect(backend, &TerminalBackend::shellExited, this, [this](){
        QString finalCwd = backend->getCwdFromProc();
        outputBox->appendPlainText(QString("\n--- Shell process exited. Final directory: %1 ---").arg(finalCwd));
        inputBox->setEnabled(false);
    });

    // Command Entry
    connect(inputBox, &QLineEdit::returnPressed, this, [this](){
        QString cmd = inputBox->text().trimmed();

        if(cmd.isEmpty()) {
            backend->sendCommand(getCwdCommand());
            return;
        }

        history.append(cmd);
        historyIndex = -1;

        // Manual Echo
        // 1. Move cursor to the very end of the output box
        outputBox->moveCursor(QTextCursor::End);

        // 2. Insert the command text
        outputBox->insertPlainText(QString("[%1] $ %2").arg(currentDir).arg(cmd));

        // 3. Explicitly insert a newline
        outputBox->insertPlainText("\n");

        // 4. Scroll to bottom
        outputBox->verticalScrollBar()->setValue(outputBox->verticalScrollBar()->maximum());

        if(cmd == "clear" || cmd == "reset") {
            outputBox->clear();
            inputBox->clear();
            updatePrompt();
            // Send *only* the clear command
            backend->sendCommand(cmd);
            return;
        }

        // Chain the user's command with the CWD command
        QString fullCommand = QString("%1; %2").arg(cmd).arg(getCwdCommand());

        backend->sendCommand(fullCommand);
        inputBox->clear();
    });
}

QString MainWindow::getCwdCommand() const {
    QString hostname = QHostInfo::localHostName();
    // No redirection. We *want* the OSC 7 sequence to be printed to stdout
    // so our backend parser can read it. Echo is already off.
    return QString("printf \"\\033]7;file://%1%s\\007\" \"$PWD\"").arg(hostname);
}

MainWindow::~MainWindow() { /* backend cleanup handled by QObject parent/child hierarchy */ }

void MainWindow::updatePrompt() {
    if(!currentDir.isEmpty()){
        inputBox->setPlaceholderText(QString("[%1] $").arg(currentDir));
    } else {
        inputBox->setPlaceholderText("$");
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event){
    if (obj == inputBox && event->type() == QEvent::KeyPress){
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if(!history.isEmpty()){
            if (keyEvent->key() == Qt::Key_Up){
                if (historyIndex == -1) historyIndex = history.size() -1;
                else if (historyIndex > 0) historyIndex--;
                inputBox->setText(history[historyIndex]);
                return true;
            }
            else if (keyEvent->key() == Qt::Key_Down){
                if(historyIndex == -1) return false;
                else if (historyIndex < history.size() - 1){
                    historyIndex++;
                    inputBox->setText(history[historyIndex]);
                } else {
                    historyIndex = -1;
                    inputBox->clear();
                }
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
