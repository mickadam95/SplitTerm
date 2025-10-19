#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QStringList>
#include "terminalbackend.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updatePrompt();

    QPlainTextEdit *outputBox;
    QLineEdit *inputBox;
    TerminalBackend *backend;

    QString currentDir;
    QStringList history;
    int historyIndex = -1;

    QString pendingCommand;  // store command until placeholder updates
};

#endif // MAINWINDOW_H
