// SPDX-License-Identifier: GPL-3.0-only
#include "window.h"
#include "practice.h"
#include "startup.h"
#include "scroll_safe_controls.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
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
    setWindowTitle("Stable Mouse"); setWindowIcon(appIcon());
    const auto available = screen()->availableGeometry().size();
    resize(std::min(700, available.width() - 40), std::min(860, available.height() - 80));
    auto readable = this->font(); readable.setPointSize(std::max(12, readable.pointSize())); setFont(readable);
    // A complete palette keeps native menus and checkbox glyphs readable too.
    QPalette colors = palette();
    colors.setColor(QPalette::Window, QColor("#faf9f3"));
    colors.setColor(QPalette::WindowText, QColor("#213e35"));
    colors.setColor(QPalette::Base, QColor("#fffef9"));
    colors.setColor(QPalette::AlternateBase, QColor("#eaf0e5"));
    colors.setColor(QPalette::Text, QColor("#213e35"));
    colors.setColor(QPalette::Button, QColor("#fffef9"));
    colors.setColor(QPalette::ButtonText, QColor("#213e35"));
    colors.setColor(QPalette::Highlight, QColor("#185e53"));
    colors.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(colors);
    setStyleSheet(R"(
        QWidget { font-size: 12pt; }
        QMainWindow, QScrollArea, QTabWidget::pane, QWidget#settingsPage { background: #faf9f3; }
        QLabel { color: #213e35; background: transparent; }
        QLabel#title { font-size: 25pt; font-weight: bold; }
        QLabel#status { background: #eaf0e5; color: #185e53; padding: 12px; border-radius: 8px; font-weight: bold; }
        QLabel#notice { background: #f0eddf; padding: 12px; border-radius: 8px; }
        QPushButton, QComboBox { min-height: 44px; padding: 4px 14px; border: 1px solid #8ba598; border-radius: 7px; background: #fffef9; color: #213e35; }
        QPushButton:hover, QComboBox:hover { background: #eaf0e5; border-color: #185e53; }
        QComboBox::drop-down { width: 30px; border: none; }
        QPushButton:checked, QPushButton#toggle { background: #185e53; color: white; border-color: #185e53; font-weight: bold; }
        QPushButton#toggle:hover, QPushButton:checked:hover { background: #124a42; }
        QPushButton:focus, QComboBox:focus, QCheckBox:focus { border: 3px solid #a74d22; }
        QPushButton#moreOptions { background: #eaf0e5; color: #185e53; font-weight: bold; }
        QPushButton#quit { border: none; background: transparent; text-decoration: underline; }
        QGroupBox { background: #fffef9; border: 1px solid #d7ddd2; border-radius: 10px; margin-top: 12px; padding: 18px 12px 12px; }
        QGroupBox::title { subcontrol-origin: margin; left: 16px; color: #185e53; }
        QCheckBox { min-height: 44px; spacing: 12px; color: #213e35; border: 3px solid transparent; }
        QCheckBox::indicator { width: 24px; height: 24px; }
        QSlider { min-height: 44px; background: transparent; }
        QSlider::groove:horizontal { height: 8px; background: #d7ddd2; border-radius: 4px; }
        QSlider::sub-page:horizontal { background: #185e53; border-radius: 4px; }
        QSlider::handle:horizontal { width: 28px; margin: -11px 0; background: #185e53; border: 2px solid #fffef9; border-radius: 15px; }
        QSlider::handle:horizontal:focus { border: 3px solid #a74d22; }
        QTabWidget::pane { border: none; border-top: 1px solid #d7ddd2; }
        QTabBar::tab { min-height: 40px; padding: 4px 20px; color: #53675d; border-bottom: 3px solid transparent; }
        QTabBar::tab:selected { color: #185e53; border-bottom: 3px solid #185e53; font-weight: bold; }
        QTabBar::tab:focus { border: 2px solid #a74d22; }
        QScrollBar:vertical { background: #eaf0e5; width: 20px; margin: 0; }
        QScrollBar::handle:vertical { background: #8ba598; min-height: 48px; border: 4px solid #eaf0e5; border-radius: 9px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )");
    auto *root = new QWidget; auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(14);
    auto *title = copy("Stable Mouse"); title->setObjectName("title"); auto font = title->font(); font.setPointSize(25); font.setBold(true); title->setFont(font);
    layout->addWidget(title);
    status = copy("Paused"); status->setObjectName("status"); font.setPointSize(15); status->setFont(font); layout->addWidget(status);
    toggleButton = new QPushButton("Turn smoothing on"); toggleButton->setObjectName("toggle"); toggleButton->setMinimumHeight(52);
    toggleButton->setAccessibleDescription("Turns system mouse stabilization on or off."); layout->addWidget(toggleButton);
    layout->addWidget(copy(backend->escapeHint()));
    notice = copy(""); notice->setObjectName("notice"); notice->hide(); layout->addWidget(notice);
    auto *tabs = new QTabWidget; layout->addWidget(tabs, 1);
    auto *controls = new QWidget; controls->setObjectName("settingsPage"); auto *controlsLayout = new QVBoxLayout(controls); controlsLayout->setSpacing(16); controlsLayout->setContentsMargins(4, 20, 12, 20);
    auto *smoothingGroup = new QGroupBox("How much smoothing?"); auto *smoothingLayout = new QVBoxLayout(smoothingGroup);
    auto *presets = new QHBoxLayout;
    auto *presetButtons = new QButtonGroup(this); presetButtons->setExclusive(true); presetButtons->setObjectName("presetButtons");
    const QList<QPair<QString, int>> values{{"Light", 25}, {"Balanced", 55}, {"Strong", 85}};
    for (const auto &preset : values) {
        auto *button = new QPushButton(preset.first); button->setMinimumHeight(48);
        button->setCheckable(true); button->setObjectName(QString("preset%1").arg(preset.second)); presetButtons->addButton(button);
        button->setAccessibleName(preset.first + " smoothing preset"); presets->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, value = preset.second] { strength->setValue(value); });
    }
    smoothingLayout->addLayout(presets);
    presetValue = copy(""); smoothingLayout->addWidget(presetValue);
    smoothingLayout->addWidget(copy("More smoothing is steadier, but takes longer to follow your hand."));
    controlsLayout->addWidget(smoothingGroup);
    auto *methodLabel = copy("How to steady movement"); controlsLayout->addWidget(methodLabel);
    centerMethod = new ScrollSafeComboBox; centerMethod->setObjectName("centerMethod");
    centerMethod->setAccessibleName("How to steady movement"); methodLabel->setBuddy(centerMethod);
    centerMethod->addItems({"Smoothing only", "Recognize shaking", "Always follow the center"});
    const int savedMode = settings.value("centerMode", settings.value("centerTracking", false).toBool() ? 1 : 0).toInt();
    centerMethod->setCurrentIndex(std::clamp(savedMode, 0, 2)); controlsLayout->addWidget(centerMethod);
    centerHelp = copy(""); controlsLayout->addWidget(centerHelp);
    auto *more = new QPushButton("More options"); more->setObjectName("moreOptions"); more->setCheckable(true);
    more->setAccessibleDescription("Show fine tuning and startup settings."); controlsLayout->addWidget(more);
    auto *advanced = new QWidget; advanced->setObjectName("advancedOptions");
    auto *advancedLayout = new QVBoxLayout(advanced); advancedLayout->setContentsMargins(0, 0, 0, 0); advancedLayout->setSpacing(12);
    advanced->hide(); controlsLayout->addWidget(advanced);
    connect(more, &QPushButton::toggled, this, [advanced, more](bool open) { advanced->setVisible(open); more->setText(open ? "Fewer options" : "More options"); });
    strengthValue = copy(""); advancedLayout->addWidget(copy("Fine tuning"));
    advancedLayout->addWidget(copy("Drag a slider or use the arrow keys to adjust. Scrolling moves the page without changing your settings."));
    advancedLayout->addWidget(strengthValue);
    strength = new ScrollSafeSlider(Qt::Horizontal); strength->setObjectName("strength"); strength->setRange(0, 100); strength->setPageStep(10);
    strength->setAccessibleName("Smoothing strength"); strength->setValue(std::clamp(settings.value("strength", 55).toInt(), 0, 100));
    advancedLayout->addWidget(strength);
    centerWindowControls = new QWidget; auto *windowLayout = new QVBoxLayout(centerWindowControls); windowLayout->setContentsMargins(0,0,0,0);
    centerWindowValue = copy(""); windowLayout->addWidget(centerWindowValue);
    centerWindow = new ScrollSafeSlider(Qt::Horizontal); centerWindow->setObjectName("centerWindow"); centerWindow->setRange(100,600); centerWindow->setSingleStep(25); centerWindow->setPageStep(50);
    centerWindow->setAccessibleName("Center window, milliseconds"); centerWindow->setValue(std::clamp(settings.value("centerWindow",250).toInt(),100,600));
    windowLayout->addWidget(centerWindow); windowLayout->addWidget(copy("A longer window uses more recent movement to find the center. It can feel steadier, but slower."));
    advancedLayout->addWidget(centerWindowControls);
    speedValue = copy(""); advancedLayout->addWidget(speedValue);
    speed = new ScrollSafeSlider(Qt::Horizontal); speed->setObjectName("speed"); speed->setRange(25, 200); speed->setPageStep(10);
    speed->setAccessibleName("Pointer speed, percent"); speed->setValue(std::clamp(settings.value("speed", 100).toInt(), 25, 200)); advancedLayout->addWidget(speed);
    advancedLayout->addWidget(copy("Pointer speed is separate from smoothing. Leave it at 100% to start."));
#ifdef Q_OS_LINUX
    auto *devices = new QGroupBox("Mouse to stabilize"); auto *deviceLayout = new QVBoxLayout(devices);
    device = new ScrollSafeComboBox; device->setAccessibleName("Mouse to stabilize");
    auto *deviceRow = new QHBoxLayout; deviceRow->addWidget(device, 1); deviceLayout->addLayout(deviceRow);
    auto *refresh = new QPushButton("Refresh"); refresh->setAccessibleName("Refresh mice"); deviceRow->addWidget(refresh);
    connect(refresh, &QPushButton::clicked, this, &Window::refreshDevices);
    controlsLayout->insertWidget(0, devices); refreshDevices();
#endif
    auto *startup = new QGroupBox("When you sign in"); auto *startupLayout = new QVBoxLayout(startup);
    login = new QCheckBox("Open when I sign in"); login->setObjectName("login"); login->setChecked(testMode ? false : Startup::enabled()); startupLayout->addWidget(login);
    auto *enabledOnLaunch = new QCheckBox("Turn smoothing on when opened"); enabledOnLaunch->setObjectName("enableOnLaunch");
    enabledOnLaunch->setChecked(settings.value("enableOnLaunch", false).toBool()); startupLayout->addWidget(enabledOnLaunch);
    advancedLayout->addWidget(startup); controlsLayout->addStretch();
    auto *scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(controls); tabs->addTab(scroll, "Settings");
    auto *practicePage = new QWidget; auto *practiceLayout = new QVBoxLayout(practicePage);
    practiceLayout->addWidget(copy("Move between the large targets and click each one. Compare how it feels with stabilization enabled and paused. Nothing here is recorded or saved."));
    auto *practice = new Practice; practiceLayout->addWidget(practice, 1);
    auto *clear = new QPushButton("Clear practice area"); practiceLayout->addWidget(clear); connect(clear, &QPushButton::clicked, practice, &Practice::clear);
    tabs->addTab(practicePage, "Practice");
    auto *about = new QWidget; auto *aboutLayout = new QVBoxLayout(about);
    aboutLayout->addWidget(copy("Stable Mouse " + QCoreApplication::applicationVersion() + " • Preview"));
    aboutLayout->addWidget(copy("Free software, licensed under GPL-3.0-only. No account, advertising, analytics, or movement history."));
    aboutLayout->addWidget(copy(backend->instructions()));
    aboutLayout->addWidget(copy("This preview needs testing on real devices. Smoothing preferences vary. The app does not assess tremor severity or provide medical measurements."));
    aboutLayout->addWidget(copy("Closing the window keeps Stable Mouse in the tray if your desktop supports it. Quit stops filtering and releases the mouse. Turn off “Open when I sign in” before uninstalling."));
    aboutLayout->addStretch(); tabs->addTab(about, "Help");
    auto *quit = new QPushButton("Quit Stable Mouse"); quit->setObjectName("quit"); quit->setMinimumHeight(36); layout->addWidget(quit);
    setCentralWidget(root);
    connect(quit, &QPushButton::clicked, qApp, &QApplication::quit);
    connect(toggleButton, &QPushButton::clicked, this, &Window::toggle);
    connect(strength, &QSlider::valueChanged, this, &Window::updateConfig);
    connect(speed, &QSlider::valueChanged, this, &Window::updateConfig);
    connect(centerMethod, qOverload<int>(&QComboBox::currentIndexChanged), this, &Window::updateConfig);
    connect(centerWindow, &QSlider::valueChanged, this, &Window::updateConfig);
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
        trayToggle = menu->addAction("Turn smoothing on", this, &Window::toggle);
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
    status->setText(on ? "Smoothing is on" : starting ? "Waiting for mouse access…" : "Paused · Your mouse moves normally");
    toggleButton->setText(on ? "Pause smoothing" : starting ? "Cancel" : "Turn smoothing on");
    if (device) device->setEnabled(!on && !starting);
    if (tray) { tray->setToolTip(on ? "Stable Mouse: on" : "Stable Mouse: paused"); trayToggle->setText(toggleButton->text()); }
}
void Window::updateConfig() {
    strengthValue->setText(QString("Smoothing strength: %1%").arg(strength->value()));
    speedValue->setText(QString("Pointer speed: %1%").arg(speed->value()));
    settings.setValue("strength", strength->value()); settings.setValue("speed", speed->value());
    const int mode = centerMethod->currentIndex();
    settings.setValue("centerTracking", mode != 0); settings.setValue("centerMode", mode);
    settings.setValue("centerWindow", centerWindow->value());
    centerWindowValue->setText(QString("Center window: %1 ms").arg(centerWindow->value()));
    centerWindowControls->setVisible(mode == 2);
    centerHelp->setText(mode == 0 ? "Softens all mouse movement." : mode == 1 ? "Looks for back-and-forth shaking before following its center." : "Follows the center of recent movement, then smooths it. This can add delay.");
    const QString preset = strength->value()==25 ? "Light" : strength->value()==55 ? "Balanced" : strength->value()==85 ? "Strong" : "Custom";
    presetValue->setText(preset + " smoothing");
    auto *buttons=findChild<QButtonGroup *>("presetButtons"); buttons->setExclusive(false);
    for (int value : {25,55,85}) { auto *button=findChild<QPushButton *>(QString("preset%1").arg(value)); QSignalBlocker block(button); button->setChecked(strength->value()==value); }
    buttons->setExclusive(true);
    backend->configure({double(strength->value()), speed->value() / 100.0, mode != 0, mode == 2, centerWindow->value()/1000.0});
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
