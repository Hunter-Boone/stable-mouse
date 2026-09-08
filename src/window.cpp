// SPDX-License-Identifier: GPL-3.0-only
#include "window.h"
#include "practice.h"
#include "startup.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QLabel *copy(const QString &text) { auto *label = new QLabel(text); label->setWordWrap(true); label->setTextFormat(Qt::PlainText); return label; }
QIcon appIcon() {
    QPixmap pix(64, 64); pix.fill(Qt::transparent);
    QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#147D74")); p.setPen(Qt::NoPen); p.drawRoundedRect(2, 2, 60, 60, 16, 16);
    p.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(14, 34, 23, 28); p.drawLine(23, 28, 32, 34); p.drawLine(32, 34, 50, 34);
    return QIcon(pix);
}
}
Window::Window(std::unique_ptr<Backend> input, bool startupLaunch, bool testMode) : backend(std::move(input)) {
    setWindowTitle("Stable Mouse"); setWindowIcon(appIcon()); resize(660, 780);
    auto *root = new QWidget; auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(14);
    auto *title = copy("Stable Mouse"); auto font = title->font(); font.setPointSize(25); font.setBold(true); title->setFont(font);
    layout->addWidget(title);
    layout->addWidget(copy("A steadier pointer, at your pace."));
    status = copy("Paused"); status->setObjectName("status"); font.setPointSize(15); status->setFont(font); layout->addWidget(status);
    toggleButton = new QPushButton("Enable stabilization"); toggleButton->setObjectName("toggle"); toggleButton->setMinimumHeight(52);
    toggleButton->setAccessibleDescription("Turns system mouse stabilization on or off."); layout->addWidget(toggleButton);
    layout->addWidget(copy(backend->escapeHint()));
    notice = copy(""); notice->setObjectName("notice"); notice->hide(); layout->addWidget(notice);
    auto *tabs = new QTabWidget; layout->addWidget(tabs, 1);
    auto *controls = new QWidget; auto *controlsLayout = new QVBoxLayout(controls); controlsLayout->setSpacing(16);
    auto *smoothingGroup = new QGroupBox("Pointer settings"); auto *smoothingLayout = new QVBoxLayout(smoothingGroup);
    auto *presets = new QHBoxLayout;
    const QList<QPair<QString, int>> values{{"Light", 25}, {"Balanced", 55}, {"Strong", 85}};
    for (const auto &preset : values) {
        auto *button = new QPushButton(preset.first); button->setMinimumHeight(36);
        button->setAccessibleName(preset.first + " smoothing preset"); presets->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, value = preset.second] { strength->setValue(value); });
    }
    smoothingLayout->addLayout(presets);
    strengthValue = copy(""); smoothingLayout->addWidget(strengthValue);
    strength = new QSlider(Qt::Horizontal); strength->setObjectName("strength"); strength->setRange(0, 100); strength->setPageStep(10);
    strength->setAccessibleName("Smoothing strength"); strength->setValue(std::clamp(settings.value("strength", 55).toInt(), 0, 100));
    smoothingLayout->addWidget(strength);
    smoothingLayout->addWidget(copy("More smoothing reduces small changes in movement, but adds delay. Start with Balanced and adjust a little at a time."));
    speedValue = copy(""); smoothingLayout->addWidget(speedValue);
    speed = new QSlider(Qt::Horizontal); speed->setObjectName("speed"); speed->setRange(25, 200); speed->setPageStep(10);
    speed->setAccessibleName("Pointer speed, percent"); speed->setValue(std::clamp(settings.value("speed", 100).toInt(), 25, 200)); smoothingLayout->addWidget(speed);
    smoothingLayout->addWidget(copy("Clicking stops any remaining drift. Button presses, scrolling, and dragging remain under your control."));
    controlsLayout->addWidget(smoothingGroup);
#ifdef Q_OS_LINUX
    auto *devices = new QGroupBox("Mouse to stabilize"); auto *deviceLayout = new QVBoxLayout(devices);
    device = new QComboBox; device->setAccessibleName("Mouse to stabilize"); deviceLayout->addWidget(device);
    auto *refresh = new QPushButton("Refresh mice"); deviceLayout->addWidget(refresh);
    connect(refresh, &QPushButton::clicked, this, &Window::refreshDevices);
    controlsLayout->addWidget(devices); refreshDevices();
#endif
    auto *startup = new QGroupBox("When you sign in"); auto *startupLayout = new QVBoxLayout(startup);
    login = new QCheckBox("Open Stable Mouse at login"); login->setObjectName("login"); login->setChecked(testMode ? false : Startup::enabled()); startupLayout->addWidget(login);
    auto *enabledOnLaunch = new QCheckBox("Enable stabilization when the app opens"); enabledOnLaunch->setObjectName("enableOnLaunch");
    enabledOnLaunch->setChecked(settings.value("enableOnLaunch", false).toBool()); startupLayout->addWidget(enabledOnLaunch);
    controlsLayout->addWidget(startup); controlsLayout->addStretch();
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(controls); tabs->addTab(scroll, "Settings");
    auto *practicePage = new QWidget; auto *practiceLayout = new QVBoxLayout(practicePage);
    practiceLayout->addWidget(copy("Move between the large targets and click each one. Compare how it feels with stabilization enabled and paused. Nothing here is recorded or saved."));
    auto *practice = new Practice; practiceLayout->addWidget(practice, 1);
    auto *clear = new QPushButton("Clear practice area"); practiceLayout->addWidget(clear); connect(clear, &QPushButton::clicked, practice, &Practice::clear);
    tabs->addTab(practicePage, "Practice");
    auto *about = new QWidget; auto *aboutLayout = new QVBoxLayout(about);
    aboutLayout->addWidget(copy("Stable Mouse 0.1.0 • Preview"));
    aboutLayout->addWidget(copy("Free software, licensed under GPL-3.0-only. No account, advertising, analytics, or movement history."));
    aboutLayout->addWidget(copy(backend->instructions()));
    aboutLayout->addWidget(copy("This preview needs testing on real devices. Smoothing preferences vary. The app does not assess tremor severity or provide medical measurements."));
    aboutLayout->addWidget(copy("Closing the window keeps Stable Mouse in the tray if your desktop supports it. Quit stops filtering and releases the mouse. Turn off “Open Stable Mouse at login” before uninstalling."));
    aboutLayout->addStretch(); tabs->addTab(about, "About");
    auto *quit = new QPushButton("Quit Stable Mouse"); quit->setMinimumHeight(36); layout->addWidget(quit);
    setCentralWidget(root);
    connect(quit, &QPushButton::clicked, qApp, &QApplication::quit);
    connect(toggleButton, &QPushButton::clicked, this, &Window::toggle);
    connect(strength, &QSlider::valueChanged, this, &Window::updateConfig);
    connect(speed, &QSlider::valueChanged, this, &Window::updateConfig);
    connect(enabledOnLaunch, &QCheckBox::toggled, this, [this](bool on) { settings.setValue("enableOnLaunch", on); });
    connect(login, &QCheckBox::toggled, this, [this, testMode](bool on) {
        if (testMode) return;
        QString error;
        if (!Startup::setEnabled(on, &error)) { QSignalBlocker block(login); login->setChecked(Startup::enabled()); showProblem(error); }
    });
    connect(backend.get(), &Backend::stateChanged, this, [this] { starting = false; syncState(); });
    connect(backend.get(), &Backend::problem, this, &Window::showProblem);
    connect(backend.get(), &Backend::emergencyPause, this, [this] { pause(); notice->setText("Paused with the emergency control."); notice->show(); reveal(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this); connect(escape, &QShortcut::activated, this, &Window::pause);
    if (!testMode && QSystemTrayIcon::isSystemTrayAvailable()) {
        tray = new QSystemTrayIcon(windowIcon(), this); auto *menu = new QMenu(this);
        menu->addAction("Open Stable Mouse", this, &Window::reveal);
        trayToggle = menu->addAction("Enable stabilization", this, &Window::toggle);
        menu->addSeparator(); menu->addAction("Quit", qApp, &QApplication::quit);
        tray->setContextMenu(menu); tray->show();
        connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) { if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) reveal(); });
    }
    updateConfig(); syncState();
    if (!testMode && enabledOnLaunch->isChecked()) QTimer::singleShot(300, this, [this] { if (!backend->active() && !starting) toggle(); });
    if (!startupLaunch || !tray) show();
}
Window::~Window() { backend->stop(); }
void Window::reveal() { showNormal(); raise(); activateWindow(); }
void Window::toggle() {
    if (backend->active() || starting) { pause(); return; }
    notice->hide(); starting = true; syncState();
    const auto selected = device ? device->currentText() : QString();
    if (device) settings.setValue("device", selected);
    if (!backend->start(selected)) { starting = false; syncState(); }
}
void Window::pause() { backend->stop(); starting = false; syncState(); }
void Window::syncState() {
    const bool on = backend->active();
    status->setText(on ? "Stabilization is on" : starting ? "Waiting for mouse access…" : "Paused · Your mouse moves normally");
    toggleButton->setText(on ? "Pause stabilization" : starting ? "Cancel" : "Enable stabilization");
    if (device) device->setEnabled(!on && !starting);
    if (tray) { tray->setToolTip(on ? "Stable Mouse: on" : "Stable Mouse: paused"); trayToggle->setText(toggleButton->text()); }
}
void Window::updateConfig() {
    strengthValue->setText(QString("Smoothing strength: %1%").arg(strength->value()));
    speedValue->setText(QString("Pointer speed: %1%").arg(speed->value()));
    settings.setValue("strength", strength->value()); settings.setValue("speed", speed->value());
    backend->configure({double(strength->value()), speed->value() / 100.0});
}
void Window::showProblem(const QString &message) {
    starting = false; syncState(); notice->setText(message); notice->show(); reveal();
}
void Window::refreshDevices() {
    if (!device || backend->active() || starting) return;
    const auto saved = device->currentText().isEmpty() ? settings.value("device").toString() : device->currentText();
    device->clear(); device->addItems(backend->devices());
    const int index = device->findText(saved); if (index >= 0) device->setCurrentIndex(index);
    if (device->count() == 0) device->setPlaceholderText("No supported mouse found");
}
void Window::closeEvent(QCloseEvent *event) {
    if (tray) { hide(); event->ignore(); }
    else { pause(); event->accept(); }
}
