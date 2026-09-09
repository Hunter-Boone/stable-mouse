// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QWidget>
#include <functional>
class Updater;
class QPushButton;

class UpdateWidget final : public QWidget {
    Q_OBJECT
public:
    explicit UpdateWidget(bool testMode, QWidget *parent = nullptr, Updater *updater = nullptr,
                          std::function<bool(const QString &)> launcher = {});
    void activateUpdate();
signals:
    void updateAvailable(bool available);
    void bannerTextChanged(const QString &text);
    void installing();
    void installerOpened();
private:
    Updater *updater;
    QPushButton *installButton;
    QPushButton *notesButton;
};
