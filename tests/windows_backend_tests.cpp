// SPDX-License-Identifier: GPL-3.0-only
// Opt-in only: temporarily installs the real hook and sends the pause shortcut.
#include "backend.h"
#include <windows.h>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (!qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_NATIVE_INPUT")) {
        std::cout << "Set STABLE_MOUSE_TEST_NATIVE_INPUT=1 on an isolated Windows test desktop.\n";
        return 77;
    }
    auto backend = createBackend();
    QObject::connect(backend.get(), &Backend::problem, [](const QString &error) { std::cerr << qPrintable(error) << '\n'; });
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
