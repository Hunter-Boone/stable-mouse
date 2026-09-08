// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "backend.h"
#include <QMainWindow>
#include <QSettings>
class QPushButton;
class QLabel;
class QSlider;
class QComboBox;
class QCheckBox;
class QSystemTrayIcon;
class QAction;
class Window final : public QMainWindow {
    Q_OBJECT
public:
    explicit Window(std::unique_ptr<Backend> backend, bool startupLaunch = false, bool testMode = false);
    ~Window() override;
    void reveal();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void toggle();
    void pause();
    void syncState();
    void updateConfig();
    void showProblem(const QString &message);
    void refreshDevices();
    std::unique_ptr<Backend> backend;
    QSettings settings;
    QPushButton *toggleButton;
    QLabel *status, *notice, *strengthValue, *speedValue, *centerHelp, *centerWindowValue, *presetValue;
    QSlider *strength, *speed, *centerWindow;
    QComboBox *centerMethod;
    QWidget *centerWindowControls;
    QComboBox *device = nullptr;
    QCheckBox *login;
    QSystemTrayIcon *tray = nullptr;
    QAction *trayToggle = nullptr;
    bool starting = false;
};
