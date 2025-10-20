#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStringList>
#include "terminalbackend.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    TerminalBackend *backend = nullptr;
    QPlainTextEdit *outputBox = nullptr;
    QLineEdit *inputBox = nullptr;
    QString currentDir;

    QStringList history;
    int historyIndex = -1;

    void updatePrompt();
    QString getCwdCommand() const;
};

#endif // MAINWINDOW_H
