#include "mainwindow.h"
#include <QApplication>
#include <QSettings> // <-- Add this

void setDefaultSettings()
{
    QSettings settings("MyCompany", "SplitTerm");

    // Only set defaults if a key doesn't exist
    if (!settings.contains("ansi/30")) {
        settings.setValue("ansi/30", QColor(Qt::black));
        settings.setValue("ansi/31", QColor(Qt::red));
        settings.setValue("ansi/32", QColor(Qt::green));
        settings.setValue("ansi/33", QColor(Qt::yellow));
        settings.setValue("ansi/34", QColor(Qt::blue));
        settings.setValue("ansi/35", QColor(Qt::magenta));
        settings.setValue("ansi/36", QColor(Qt::cyan));
        settings.setValue("ansi/37", QColor(Qt::white));
        // Add 90-97 (bright) defaults too
        settings.setValue("ansi/90", QColor(Qt::gray));
        settings.setValue("ansi/91", QColor("orangered"));
        settings.setValue("ansi/92", QColor("limegreen"));
        settings.setValue("ansi/93", QColor("yellow"));
        settings.setValue("ansi/94", QColor("deepskyblue"));
        settings.setValue("ansi/95", QColor("magenta"));
        settings.setValue("ansi/96", QColor("cyan"));
        settings.setValue("ansi/97", QColor(Qt::white));
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // --- ADD THESE LINES ---
    QCoreApplication::setOrganizationName("MyCompany");
    QCoreApplication::setApplicationName("SplitTerm");

    // Set default colors on first launch
    setDefaultSettings();
    // --- END ADD ---

    MainWindow w;
    w.show();
    return a.exec();
}
