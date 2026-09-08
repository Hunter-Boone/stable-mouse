// SPDX-License-Identifier: GPL-3.0-only
// Opt-in only: temporarily installs the real hook and sends the pause shortcut.
#include "backend.h"
#include <windows.h>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
#include <fstream>
#include <QPainter>
#include <QWidget>
#include <QMouseEvent>
#include "motion_scenarios.h"

// Visualize the unfiltered path too: the real cursor alone only shows output.
class ReplayView : public QWidget {
public:
    int presses = 0, releases = 0, dragMoves = 0;
    void mousePressEvent(QMouseEvent *) override { ++presses; }
    void mouseReleaseEvent(QMouseEvent *) override { ++releases; }
    void mouseMoveEvent(QMouseEvent *event) override { if (event->buttons() & Qt::LeftButton) ++dragMoves; }
    scenarios::Sample input{}; Motion output{}; QString scenario;
    void paintEvent(QPaintEvent *) override {
        QPainter p(this); p.fillRect(rect(), Qt::white);
        p.setPen(Qt::black); p.drawText(30, 35, "Stable Mouse engineering replay. Please leave the mouse untouched.");
        p.drawText(30, 60, scenario + " | Strong 85% | plotted at half scale");
        p.drawText(30, 85, "Red: raw input    Blue: filtered Windows cursor    Black: intended target");
        p.translate(width()/2, height()/2); p.scale(.5, .5);
        p.setPen(QPen(Qt::black, 3)); p.drawEllipse(QPointF(input.intended.x, input.intended.y), 12, 12);
        p.setPen(QPen(Qt::red, 4)); p.drawEllipse(QPointF(input.raw.x, input.raw.y), 15, 15);
        p.setPen(QPen(Qt::blue, 4)); p.drawLine(QPointF(output.x-12, output.y), QPointF(output.x+12, output.y));
        p.drawLine(QPointF(output.x, output.y-12), QPointF(output.x, output.y+12));
    }
};

bool replayShake(Backend &backend, const scenarios::Scenario &scenario, ReplayView &view, std::ofstream &trace) {
    POINT original{};
    if (!GetCursorPos(&original)) return false;
    struct Restore {
        Backend &backend; POINT point;
        ~Restore() { backend.stop(); SetCursorPos(point.x, point.y); }
    } restore{backend, original};
    const POINT center{GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2};
    SetCursorPos(center.x, center.y);
    backend.configure({85, 1, qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_CENTER"), qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_ALWAYS"), .25});
    if (!backend.start({})) return false;
    view.scenario = QString(scenario.name) + (qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_CENTER") ? " | center tracking on" : "");
    QEventLoop loop; QTimer timer; QElapsedTimer elapsed;
    timer.setTimerType(Qt::PreciseTimer); timer.setInterval(8);
    Motion previous{}; scenarios::Metrics metrics;
    bool injectionOk = true;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
        const double t = elapsed.nsecsElapsed() / 1e9;
        POINT actual{};
        if (!backend.active() || !GetCursorPos(&actual)) { injectionOk = false; loop.quit(); return; }
        const auto input = scenarios::sample(scenario, t);
        const Motion output{double(actual.x-center.x), double(actual.y-center.y)};
        metrics.add(t, input, output);
        view.input = input; view.output = output; view.update();
        if (trace) trace << scenario.name << ",85," << t << ',' << input.raw.x << ',' << input.raw.y << ',' << input.intended.x << ',' << input.intended.y << ',' << output.x << ',' << output.y << '\n';
        if (t >= 6) { loop.quit(); return; }
        if (!backend.replayMotion(int(input.raw.x-previous.x), int(input.raw.y-previous.y))) { injectionOk = false; loop.quit(); }
        previous = input.raw;
    });
    elapsed.start(); timer.start(); loop.exec(); timer.stop();
    std::cout << scenario.name << ": raw error RMS=" << metrics.rawRms() << ", output error RMS=" << metrics.errorRms()
              << ", ratio=" << (scenario.kind == 2 && scenario.amplitude == 0 ? -1 : metrics.ratio()) << ", peak error=" << metrics.peak << ", target dwell=" << metrics.dwell() << '\n';
    const bool withinBound = scenario.amplitude || scenario.kind == 1 ? metrics.ratio() < scenario.maxErrorRatio : metrics.errorRms() < 70;
    return injectionOk && metrics.count > 100 && withinBound;
}

void pump(int milliseconds) {
    QEventLoop loop; QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit); loop.exec();
}
bool clickAndDrag(Backend &backend, ReplayView &view) {
    POINT original{}; if (!GetCursorPos(&original)) return false;
    view.scenario = "Click reset and drag in this test window";
    view.raise(); view.activateWindow(); pump(100);
    const auto start = view.mapToGlobal(QPoint(300,350));
    SetCursorPos(start.x(),start.y());
    const auto inside = [&] {
        POINT p{};return GetCursorPos(&p) && GetAncestor(WindowFromPoint(p),GA_ROOT)==reinterpret_cast<HWND>(view.winId());
    };
    if (!inside()) { SetCursorPos(original.x,original.y); return false; }
    backend.configure({85,1,true,qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_ALWAYS"),.25});
    if (!backend.start({})) { SetCursorPos(original.x,original.y);return false; }
    bool ok = backend.replayMotion(80,0);pump(40);
    ok = ok && inside();
    bool down = false;
    if (ok) {down=backend.replayButton(true);ok=down;}
    pump(80);POINT clicked{};GetCursorPos(&clicked);
    pump(200);POINT held{};GetCursorPos(&held);
    ok = ok && view.presses==1 && std::abs(held.x-clicked.x)<=2 && std::abs(held.y-clicked.y)<=2;
    if (ok) {ok=backend.replayMotion(100,0);pump(1600);}
    POINT dragged{};GetCursorPos(&dragged);
    ok = ok && std::abs(dragged.x-held.x-100)<=3 && view.dragMoves>0;
    if (down) {backend.replayButton(false);pump(100);}
    ok = ok && view.releases==1;
    backend.stop();SetCursorPos(original.x,original.y);
    std::cout<<"Click/drag: presses="<<view.presses<<", releases="<<view.releases<<", drag moves="<<view.dragMoves<<", travel="<<dragged.x-held.x<<'\n';
    return ok;
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (!qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_NATIVE_INPUT")) {
        std::cout << "Set STABLE_MOUSE_TEST_NATIVE_INPUT=1 on an isolated Windows test desktop.\n";
        return 77;
    }
    auto backend = createBackend();
    QObject::connect(backend.get(), &Backend::problem, [](const QString &error) { std::cerr << qPrintable(error) << '\n'; });
    std::ofstream trace;
    const auto tracePath = qEnvironmentVariable("STABLE_MOUSE_TEST_TRACE");
    if (!tracePath.isEmpty()) {
        trace.open(tracePath.toStdString());
        if (!trace) return 1;
        trace << "scenario,strength,t,raw_x,raw_y,intended_x,intended_y,output_x,output_y\n";
    }
    ReplayView view; view.resize(1000, 650); view.setWindowTitle("Stable Mouse replay test"); view.show();
    app.processEvents();
    for (const auto &scenario : scenarios::cases) {
        if (!replayShake(*backend, scenario, view, trace)) { std::cerr << "Native motion scenario failed.\n"; return 1; }
    }
    if (!clickAndDrag(*backend, view)) { std::cerr << "Click reset or drag forwarding failed.\n"; return 1; }
    view.hide();
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
