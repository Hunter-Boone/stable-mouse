// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QWidget>
#include <functional>
class Updater;

class UpdateWidget final : public QWidget {
    Q_OBJECT
public:
    explicit UpdateWidget(bool testMode, QWidget *parent = nullptr, Updater *updater = nullptr,
                          std::function<bool(const QString &)> launcher = {});
signals:
    void updateAvailable(bool available);
    void installing();
    void installerOpened();
};
