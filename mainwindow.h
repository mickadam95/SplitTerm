#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QStringList>
#include "terminalbackend.h"

class QTextEdit;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void showSettingsDialog(); // Slot to open the settings window

private:
    TerminalBackend *backend = nullptr;
    QTextEdit *outputBox = nullptr;      // Changed to QTextEdit for HTML
    QPlainTextEdit *inputBox = nullptr; // For multi-line input
    QString currentDir;

    QStringList history;
    int historyIndex = -1;

    void updatePrompt();
    QString getCwdCommand() const;

    void handleCommand(const QString &cmd);

    QAction *m_settingsAction; // Menu action for settings
};

#endif // MAINWINDOW_H
