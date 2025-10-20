#include "mainwindow.h"
#include "settingsdialog.h" // Include the new dialog
#include <QVBoxLayout>
#include <QScrollBar>
#include <QKeyEvent>
#include <QTextCursor>
#include <QTimer>
#include <QHostInfo>
#include <QTextEdit>     // Include for QTextEdit
#include <QMenuBar>      // Include for menu bar
#include <QAction>       // Include for QAction
#include <QFont>         // Include for QFont

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    // Output box is now QTextEdit for HTML
    outputBox = new QTextEdit;
    outputBox->setReadOnly(true);
    outputBox->setPlaceholderText("Command output will appear here...");
    outputBox->setFontFamily("Monospace");

    // Input box is QPlainTextEdit for multi-line
    inputBox = new QPlainTextEdit;
    inputBox->setPlaceholderText("Type a shell command (Shift+Enter for newline)...");
    inputBox->installEventFilter(this);
    inputBox->setMaximumHeight(80);

    QFont monoFont("Monospace");
    inputBox->setFont(monoFont);

    layout->addWidget(outputBox);
    layout->addWidget(inputBox);
    central->setLayout(layout);
    setCentralWidget(central);
    resize(800, 600);

    // --- ADD MENU BAR ---
    m_settingsAction = new QAction("&Settings...", this);
    QMenu *editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(m_settingsAction);

    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::showSettingsDialog);
    // --- END MENU BAR ---

    backend = new TerminalBackend(this);
    backend->startShell("/bin/bash");

    backend->sendCommand(getCwdCommand());

    // Fallback timer
    QTimer::singleShot(500, this, [this](){
        if(currentDir.isEmpty()) {
            QString procCwd = backend->getCwdFromProc();
            if (!procCwd.isEmpty()) {
                currentDir = procCwd;
                updatePrompt();
                outputBox->append(QString("<i>Note: CWD set via /proc fallback: %1</i>").arg(procCwd));
            }
        }
    });

    // CONNECTS
    // Connect to the new readyReadHtml signal
    connect(backend, &TerminalBackend::readyReadHtml, this, [this](const QString &html){
        outputBox->moveCursor(QTextCursor::End);
        outputBox->insertHtml(html); // Use insertHtml
        outputBox->verticalScrollBar()->setValue(outputBox->verticalScrollBar()->maximum());
    });

    connect(backend, &TerminalBackend::pwdOutput, this, [this](const QString &dir){
        currentDir = dir;
        updatePrompt();
    });

    connect(backend, &TerminalBackend::shellExited, this, [this](){
        QString finalCwd = backend->getCwdFromProc();
        outputBox->append(QString("<br><i>--- Shell process exited. Final directory: %1 ---</i>").arg(finalCwd));
        inputBox->setEnabled(false);
    });
}

MainWindow::~MainWindow() { /* QObject hierarchy will delete children */ }

// New slot to show the settings dialog
void MainWindow::showSettingsDialog()
{
    SettingsDialog dialog(this);
    // dialog.exec() shows the window modally
    if (dialog.exec() == QDialog::Accepted) {
        // User clicked OK, so settings were saved.
        // Tell the backend to reload the new colors.
        backend->loadColorSettings();
    }
}

// Helper function to build the CWD command
QString MainWindow::getCwdCommand() const {
    QString hostname = QHostInfo::localHostName();
    // This command prints the OSC 7 sequence to stdout for our parser
    return QString("printf \"\\033]7;file://%1%s\\007\" \"$PWD\"").arg(hostname);
}

void MainWindow::updatePrompt() {
    if(!currentDir.isEmpty()){
        inputBox->setPlaceholderText(QString("[%1] $").arg(currentDir));
    } else {
        inputBox->setPlaceholderText("$");
    }
}

// Function to handle command logic
void MainWindow::handleCommand(const QString &cmd) {
    if(cmd.isEmpty()) {
        backend->sendCommand(getCwdCommand());
        return;
    }

    history.append(cmd);
    historyIndex = -1;

    // Echo command as HTML
    outputBox->moveCursor(QTextCursor::End);

    // --- FIX 3: Simplify HTML escaping ---
    QString escapedCmd = cmd;
    escapedCmd.replace(QLatin1String("&"), QLatin1String("&amp;"));
    escapedCmd.replace(QLatin1String("\n"), QLatin1String("<br>"));

    outputBox->insertHtml(QString("<b>[%1] $</b> %2<br>").arg(currentDir).arg(escapedCmd));
    outputBox->verticalScrollBar()->setValue(outputBox->verticalScrollBar()->maximum());

    if(cmd == "clear" || cmd == "reset") {
        outputBox->clear();
        inputBox->clear();
        updatePrompt();
        backend->sendCommand(cmd);
        return;
    }

    QString fullCommand = QString("%1; %2").arg(cmd).arg(getCwdCommand());
    backend->sendCommand(fullCommand);
    inputBox->clear();
}


// Event filter for multi-line input and history
bool MainWindow::eventFilter(QObject *obj, QEvent *event){
    if (obj == inputBox && event->type() == QEvent::KeyPress){
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // Handle Enter and Shift+Enter
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                // Shift+Enter: Let Qt handle it (inserts a newline)
                return QMainWindow::eventFilter(obj, event);
            } else {
                // Just Enter: Send the command
                QString cmd = inputBox->toPlainText().trimmed();
                handleCommand(cmd);
                return true; // We handled the event
            }
        }

        // History (Up/Down keys)
        if(!history.isEmpty()){
            if (keyEvent->key() == Qt::Key_Up){
                if (historyIndex == -1) historyIndex = history.size() -1;
                else if (historyIndex > 0) historyIndex--;
                inputBox->setPlainText(history[historyIndex]);
                // Move cursor to end
                QTextCursor cursor = inputBox->textCursor();
                cursor.movePosition(QTextCursor::End);
                inputBox->setTextCursor(cursor);
                return true;
            }
            else if (keyEvent->key() == Qt::Key_Down){
                if(historyIndex == -1) return false;
                else if (historyIndex < history.size() - 1){
                    historyIndex++;
                    inputBox->setPlainText(history[historyIndex]);
                    // Move cursor to end
                    QTextCursor cursor = inputBox->textCursor();
                    cursor.movePosition(QTextCursor::End);
                    inputBox->setTextCursor(cursor);
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
