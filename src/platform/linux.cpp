// SPDX-License-Identifier: GPL-3.0-only
#include "backend.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTimer>

namespace {
class LinuxBackend final : public Backend {
public:
    LinuxBackend() {
        heartbeat.setInterval(400);
        connect(&heartbeat, &QTimer::timeout, this, [this] { process.write("PING\n"); });
        connect(&process, &QProcess::readyReadStandardOutput, this, [this] {
            output += process.readAllStandardOutput();
            while (output.contains('\n')) {
                const auto line = output.left(output.indexOf('\n')); output.remove(0, line.size() + 1);
                if (line == "READY") setActive(true);
                if (line == "PAUSED") emit emergencyPause();
            }
        });
        connect(&process, &QProcess::readyReadStandardError, this, [this] { errors += process.readAllStandardError(); });
        connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus) {
            heartbeat.stop(); setActive(false);
            if (!stopping && code != 0) emit problem("The mouse filter could not continue. " + QString::fromUtf8(errors).trimmed());
        });
        connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) emit problem("Could not launch the input helper. Install the Stable Mouse package and pkexec.");
        });
    }
    ~LinuxBackend() override { stop(); }
    QStringList devices() const override {
        QStringList result;
        QDir devices("/sys/class/input");
        for (const auto &entry : devices.entryList({"event*"}, QDir::Dirs)) {
            QFile name(devices.filePath(entry + "/device/name"));
            if (!name.open(QIODevice::ReadOnly)) continue;
            const QString label = QString::fromUtf8(name.readAll()).trimmed();
            if (label.startsWith("Stable Mouse")) continue;
            // Use udev's mouse classification without reading device events.
            QFile devFile(devices.filePath(entry + "/dev"));
            if (!devFile.open(QIODevice::ReadOnly)) continue;
            QFile udev("/run/udev/data/c" + QString::fromUtf8(devFile.readAll()).trimmed());
            if (!udev.open(QIODevice::ReadOnly)) continue;
            const auto data = udev.readAll();
            if (data.contains("E:ID_INPUT_MOUSE=1\n") && !data.contains("E:ID_INPUT_KEYBOARD=1\n"))
                result << "/dev/input/" + entry + " | " + label;
        }
        return result;
    }
    QString instructions() const override { return "Select a relative mouse. Linux asks for administrator authentication when you enable filtering. Touchpads and tablets are not supported yet. Your keyboard and other mice stay available."; }
    QString escapeHint() const override { return "Hold the left and right mouse buttons together for 2 seconds to pause. Esc also pauses while this window has focus."; }
    void configure(FilterConfig c) override {
        config = c;
        if (process.state() == QProcess::Running) process.write(command());
    }
    bool start(const QString &device) override {
        if (process.state() != QProcess::NotRunning) return true;
        if (!devices().contains(device)) { emit problem("Choose an available mouse, then try again. Use Refresh if you just connected it."); return false; }
        const auto local = QCoreApplication::applicationDirPath() + "/stable-mouse-input";
        const auto installed = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../libexec/stable-mouse-input");
        const auto helper = QFile::exists(local) ? local : installed;
        if (!QFile::exists(helper)) { emit problem("The input helper is missing. Reinstall Stable Mouse."); return false; }
        errors.clear(); output.clear(); stopping = false;
        process.start("pkexec", {helper, device.section(" | ", 0, 0), QString::number(config.strength), QString::number(config.speed), config.centerTracking ? "1" : "0"});
        heartbeat.start(); return true;
    }
    void stop() override {
        stopping = true; heartbeat.stop();
        if (process.state() != QProcess::NotRunning) {
            process.write("STOP\n"); process.closeWriteChannel();
            if (!process.waitForFinished(1200)) { process.terminate(); if (!process.waitForFinished(1000)) { process.kill(); process.waitForFinished(1000); } }
        }
        setActive(false);
    }
private:
    QByteArray command() const { return "CONFIG " + QByteArray::number(config.strength) + " " + QByteArray::number(config.speed) + (config.centerTracking ? " 1\n" : " 0\n"); }
    FilterConfig config;
    QProcess process; QTimer heartbeat;
    QByteArray errors, output;
    bool stopping = false;
};
}
std::unique_ptr<Backend> createBackend() { return std::make_unique<LinuxBackend>(); }
