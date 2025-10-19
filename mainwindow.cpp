#include "mainwindow.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QKeyEvent>

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

    // Send silent pwd at startup to initialize placeholder
    backend->sendSilentCommand("pwd");

    connect(backend, &TerminalBackend::readyReadOutput, this, [this](const QByteArray &data){
        outputBox->moveCursor(QTextCursor::End);
        outputBox->insertPlainText(QString::fromUtf8(data));
        outputBox->moveCursor(QTextCursor::End);
        outputBox->verticalScrollBar()->setValue(outputBox->verticalScrollBar()->maximum());
    });

    connect(backend, &TerminalBackend::pwdOutput, this, [this](const QString &dir){
        currentDir = dir;
        updatePrompt();

        // If there’s a pending command, send it now that placeholder is updated
        if(!pendingCommand.isEmpty()) {
            outputBox->appendPlainText(QString("[%1] $").arg(currentDir));
            backend->sendCommand(pendingCommand);
            pendingCommand.clear();
        }
    });

    connect(inputBox, &QLineEdit::returnPressed, this, [this](){
        QString cmd = inputBox->text().trimmed();
        if(cmd.isEmpty()) return;

        history.append(cmd);
        historyIndex = -1;

        if(cmd == "clear" || cmd == "reset") {
            outputBox->clear();
            inputBox->clear();
            updatePrompt();
            return;
        }

        // Display user-entered command with currentDir
        outputBox->appendPlainText(QString("[%1] $").arg(currentDir));

        // If cd, execute it and then silently update placeholder
        if(cmd.startsWith("cd ")) {
            backend->sendCommand(cmd);        // execute cd
            backend->sendSilentCommand("pwd"); // update placeholder asynchronously
        } else {
            backend->sendCommand(cmd);        // normal command
        }

        inputBox->clear();
    });

}

MainWindow::~MainWindow() { delete backend; }

void MainWindow::updatePrompt() {
    if(!currentDir.isEmpty()){
        inputBox->setPlaceholderText(QString("[%1] $").arg(currentDir));
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
