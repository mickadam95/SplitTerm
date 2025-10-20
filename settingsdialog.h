#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>
#include <QColor>
#include <QPushButton>
#include <QMap>

// Forward declare the UI class
namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    // A single slot to handle all color button clicks
    void onColorButtonClicked();

    // Saves settings when "OK" is clicked
    void saveSettings();

private:
    void loadSettings();
    void updateButtonColor(QPushButton *button, const QColor &color);

    Ui::SettingsDialog *ui;
    QSettings m_settings;

    // Map to track our UI buttons and their associated colors
    QMap<QPushButton*, QColor> m_colorButtonMap;

    // Map to link buttons to their QSettings key
    QMap<QPushButton*, QString> m_buttonKeyMap;
};

#endif // SETTINGSDIALOG_H
