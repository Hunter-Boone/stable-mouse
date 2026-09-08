// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "filter.h"
#include <QObject>
#include <QStringList>
#include <memory>

class Backend : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool start(const QString &device) = 0;
    virtual void stop() = 0;
    virtual void configure(FilterConfig config) = 0;
    virtual QStringList devices() const { return {}; }
    virtual QString instructions() const = 0;
    virtual QString escapeHint() const = 0;
    bool active() const { return active_; }
#ifdef STABLE_MOUSE_TEST_REPLAY
    virtual bool replayMotion(int x, int y) = 0;
#endif
signals:
    void stateChanged(bool active);
    void problem(const QString &message);
    void emergencyPause();
protected:
    void setActive(bool value) { if (active_ != value) { active_ = value; emit stateChanged(value); } }
private:
    bool active_ = false;
};
std::unique_ptr<Backend> createBackend();
