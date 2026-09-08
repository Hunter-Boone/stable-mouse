// SPDX-License-Identifier: GPL-3.0-only
// Opt-in only: temporarily installs the real hook and sends the pause shortcut.
#include "backend.h"
#include <windows.h>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>

bool replayShake(Backend &backend) {
    POINT original{};
    if (!GetCursorPos(&original)) return false;
    struct Restore {
        Backend &backend; POINT point;
        ~Restore() { backend.stop(); SetCursorPos(point.x, point.y); }
    } restore{backend, original};
    const POINT center{GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2};
    SetCursorPos(center.x, center.y);
    backend.configure({85, 1});
    if (!backend.start({})) return false;
    QEventLoop loop; QTimer timer; QElapsedTimer elapsed;
    timer.setTimerType(Qt::PreciseTimer); timer.setInterval(8);
    int previous = 0, count = 0; double squared = 0;
    bool injectionOk = true;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        const double t = elapsed.nsecsElapsed() / 1e9;
        POINT actual{};
        if (!GetCursorPos(&actual)) { injectionOk = false; loop.quit(); return; }
        if (t > 1.5) { squared += std::pow(double(actual.x - center.x), 2); ++count; }
        if (t >= 4) { loop.quit(); return; }
        const int raw = int(std::llround(150 * std::sin(2 * std::acos(-1.) * 4 * t)));
        const int delta = raw - previous; previous = raw;
        const int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        INPUT event{}; event.type = INPUT_MOUSE;
        event.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        event.mi.dx = LONG((actual.x + delta - left + .5) * 65536. / width);
        event.mi.dy = LONG((actual.y - top + .5) * 65536. / height);
        event.mi.dwExtraInfo = 0x53544D54;
        if (SendInput(1, &event, sizeof(event)) != 1) { injectionOk = false; loop.quit(); }
    });
    elapsed.start(); timer.start(); loop.exec(); timer.stop();
    const double ratio = count ? std::sqrt(squared / count) / (150 / std::sqrt(2.)) : 1;
    std::cout << "Windows cursor replay: 4 Hz, 150 px amplitude, Strong, output/input RMS=" << ratio << '\n';
    return injectionOk && count > 30 && ratio < .12;
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (!qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_NATIVE_INPUT")) {
        std::cout << "Set STABLE_MOUSE_TEST_NATIVE_INPUT=1 on an isolated Windows test desktop.\n";
        return 77;
    }
    auto backend = createBackend();
    QObject::connect(backend.get(), &Backend::problem, [](const QString &error) { std::cerr << qPrintable(error) << '\n'; });
    if (!replayShake(*backend)) { std::cerr << "Native shake attenuation failed.\n"; return 1; }
    backend->configure({55, 1});
    for (int i = 0; i < 3; ++i) {
        if (!backend->start({}) || !backend->active()) return 1;
        backend->stop();
        if (backend->active()) return 1;
    }
    if (!backend->start({})) return 1;
    bool paused = false;
    QEventLoop loop;
    QObject::connect(backend.get(), &Backend::emergencyPause, &loop, [&] { paused = true; loop.quit(); });
    QTimer::singleShot(100, &loop, [&] {
        INPUT keys[6]{};
        const WORD codes[] = {VK_CONTROL, VK_MENU, VK_F8, VK_F8, VK_MENU, VK_CONTROL};
        for (int i = 0; i < 6; ++i) {
            keys[i].type = INPUT_KEYBOARD; keys[i].ki.wVk = codes[i];
            if (i >= 3) keys[i].ki.dwFlags = KEYEVENTF_KEYUP;
        }
        if (SendInput(6, keys, sizeof(INPUT)) != 6) loop.quit();
    });
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();
    const bool success = paused && !backend->active();
    backend->stop();
    if (!success) { std::cerr << "Native emergency pause did not complete.\n"; return 1; }
    std::cout << "PASS: native Windows hook start/stop, reactivation, and emergency shortcut\n";
}
