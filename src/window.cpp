// SPDX-License-Identifier: GPL-3.0-only
#include "window.h"
#include "practice.h"
#include "startup.h"
#include "update_widget.h"
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
#include <QRadioButton>
#include <QDesktopServices>
#include <QDialog>
#include <QUrl>
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
// Keep the native checkbox/radio accessibility roles and keyboard behavior,
// but make the entire styled row a hit target, including its empty space.
class RowCheckBox final : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
};
class RowRadioButton final : public QRadioButton {
public:
    using QRadioButton::QRadioButton;
protected:
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
};
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
        QMainWindow, QScrollArea, QTabWidget::pane, QWidget#settingsPage, QWidget#helpPage { background: #faf9f3; }
        QLabel { color: #213e35; background: transparent; }
        QLabel#title { font-size: 25pt; font-weight: bold; }
        QLabel#status { background: #eaf0e5; color: #185e53; padding: 12px; border-radius: 8px; font-weight: bold; }
        QLabel#notice { background: #f0eddf; padding: 12px; border-radius: 8px; }
        QPushButton, QComboBox { min-height: 44px; padding: 4px 14px; border: 1px solid #8ba598; border-radius: 7px; background: #fffef9; color: #213e35; }
        QPushButton:hover, QComboBox:hover { background: #eaf0e5; border-color: #185e53; }
        QComboBox::drop-down { width: 30px; border: none; }
        QComboBox::down-arrow { image: url(:/icons/chevron.xpm); width: 14px; height: 9px; }
        QPushButton:checked, QPushButton#toggle { background: #185e53; color: white; border-color: #185e53; font-weight: bold; }
        QPushButton#toggle:hover, QPushButton:checked:hover { background: #124a42; }
        QPushButton:focus, QComboBox:focus, QCheckBox:focus, QRadioButton:focus { border: 3px solid #a74d22; }
        QGroupBox { background: #fffef9; border: 1px solid #d7ddd2; border-radius: 10px; margin-top: 12px; padding: 18px 12px 12px; }
        QGroupBox::title { subcontrol-origin: margin; left: 16px; color: #185e53; }
        QCheckBox, QRadioButton { min-height: 64px; padding: 8px 16px; spacing: 16px; color: #213e35; border: 3px solid #d7ddd2; border-radius: 8px; background: #fffef9; }
        QCheckBox:hover, QRadioButton:hover { background: #eaf0e5; border-color: #8ba598; }
        QCheckBox:checked, QRadioButton:checked { background: #eaf0e5; border-color: #185e53; }
        QRadioButton::indicator { width: 28px; height: 28px; border: 2px solid #8ba598; border-radius: 16px; background: #fffef9; }
        QRadioButton::indicator:checked { background: #185e53; border-color: #185e53; image: url(:/icons/check.xpm); }
        QCheckBox::indicator { width: 28px; height: 28px; border: 2px solid #8ba598; border-radius: 4px; background: #fffef9; }
        QCheckBox::indicator:checked { background: #185e53; border-color: #185e53; image: url(:/icons/check.xpm); }
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
    toggleButton = new QPushButton("Turn Stability On"); toggleButton->setObjectName("toggle"); toggleButton->setMinimumHeight(52);
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
    smoothingLayout->addWidget(copy("More smoothing is steadier, but takes longer to follow your hand."));
    controlsLayout->addWidget(smoothingGroup);
    auto *methods = new QGroupBox("How to steady movement");
    auto *methodsLayout = new QVBoxLayout(methods); methodsLayout->setSpacing(12);
    centerMethod = new QButtonGroup(this); centerMethod->setObjectName("centerMethod");
    centerMethod->setExclusive(true);
    const QStringList methodNames{"Smoothing only", "Recognize shaking", "Always follow the center"};
    const int savedMode = settings.value("centerMode", settings.value("centerTracking", false).toBool() ? 1 : 0).toInt();
    for (int mode = 0; mode < methodNames.size(); ++mode) {
        auto *choice = new RowRadioButton(methodNames[mode]);
        choice->setObjectName(QString("method%1").arg(mode));
        centerMethod->addButton(choice, mode); methodsLayout->addWidget(choice);
        choice->setChecked(mode == std::clamp(savedMode, 0, 2));
    }
    controlsLayout->addWidget(methods);
    centerHelp = copy(""); methodsLayout->addWidget(centerHelp);
    strengthValue = copy(""); smoothingLayout->addWidget(strengthValue);
    strength = new ScrollSafeSlider(Qt::Horizontal); strength->setObjectName("strength"); strength->setRange(0, 100); strength->setPageStep(10);
    strength->setAccessibleName("Smoothing strength"); strength->setValue(std::clamp(settings.value("strength", 55).toInt(), 0, 100));
    smoothingLayout->addWidget(strength);
    centerWindowControls = new QWidget; auto *windowLayout = new QVBoxLayout(centerWindowControls); windowLayout->setContentsMargins(0,0,0,0);
    centerWindowValue = copy(""); windowLayout->addWidget(centerWindowValue);
    centerWindow = new ScrollSafeSlider(Qt::Horizontal); centerWindow->setObjectName("centerWindow"); centerWindow->setRange(100,600); centerWindow->setSingleStep(25); centerWindow->setPageStep(50);
    centerWindow->setAccessibleName("Center window, milliseconds"); centerWindow->setValue(std::clamp(settings.value("centerWindow",250).toInt(),100,600));
    windowLayout->addWidget(centerWindow); windowLayout->addWidget(copy("A longer window uses more recent movement to find the center. It can feel steadier, but slower."));
    methodsLayout->addWidget(centerWindowControls);
    auto *speedGroup = new QGroupBox("Pointer speed"); speedGroup->setObjectName("pointerSpeedGroup");
    auto *speedLayout = new QVBoxLayout(speedGroup); controlsLayout->addWidget(speedGroup);
    speedValue = copy(""); speedLayout->addWidget(speedValue);
    speed = new ScrollSafeSlider(Qt::Horizontal); speed->setObjectName("speed"); speed->setRange(25, 200); speed->setPageStep(10);
    speed->setAccessibleName("Pointer speed, percent"); speed->setValue(std::clamp(settings.value("speed", 100).toInt(), 25, 200)); speedLayout->addWidget(speed);
    speedLayout->addWidget(copy("Pointer speed is separate from smoothing. Leave it at 100% to start."));
#ifdef Q_OS_LINUX
    auto *devices = new QGroupBox("Mouse to stabilize"); auto *deviceLayout = new QVBoxLayout(devices);
    device = new ScrollSafeComboBox; device->setAccessibleName("Mouse to stabilize");
    auto *deviceRow = new QHBoxLayout; deviceRow->addWidget(device, 1); deviceLayout->addLayout(deviceRow);
    auto *refresh = new QPushButton("Refresh"); refresh->setAccessibleName("Refresh mice"); deviceRow->addWidget(refresh);
    connect(refresh, &QPushButton::clicked, this, &Window::refreshDevices);
    controlsLayout->insertWidget(0, devices); refreshDevices();
#endif
    auto *startup = new QGroupBox("When you sign in"); auto *startupLayout = new QVBoxLayout(startup);
    login = new RowCheckBox("Open when I sign in"); login->setObjectName("login"); login->setChecked(testMode ? false : Startup::enabled()); startupLayout->addWidget(login);
    auto *enabledOnLaunch = new RowCheckBox("Turn Mouse Stability on when opened"); enabledOnLaunch->setObjectName("enableOnLaunch");
    enabledOnLaunch->setChecked(settings.value("enableOnLaunch", false).toBool()); startupLayout->addWidget(enabledOnLaunch);
    controlsLayout->addWidget(startup); controlsLayout->addStretch();
    auto *scroll = new QScrollArea; scroll->setObjectName("settingsScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(controls); tabs->addTab(scroll, "Settings");
    auto *practicePage = new QWidget; auto *practiceLayout = new QVBoxLayout(practicePage);
    practiceLayout->addWidget(copy("Move between the large targets and click each one. Compare how it feels with Stability on and off. Nothing here is recorded or saved."));
    auto *practice = new Practice; practiceLayout->addWidget(practice, 1);
    auto *clear = new QPushButton("Clear practice area"); practiceLayout->addWidget(clear); connect(clear, &QPushButton::clicked, practice, &Practice::clear);
    tabs->addTab(practicePage, "Practice");
    auto *about = new QWidget; about->setObjectName("helpPage"); auto *aboutLayout = new QVBoxLayout(about);
    aboutLayout->addWidget(copy("Stable Mouse " + QCoreApplication::applicationVersion() + " • Preview"));
    aboutLayout->addWidget(copy("Free software, licensed under GPL-3.0-only. No account, advertising, analytics, or movement history."));
    auto *updatesGroup = new QGroupBox("Updates"); auto *updatesLayout = new QVBoxLayout(updatesGroup);
    auto *updates = new UpdateWidget(testMode); updatesLayout->addWidget(updates); aboutLayout->addWidget(updatesGroup);
    auto *updateBanner = new QPushButton("An update is available · Download update");
    updateBanner->setObjectName("updateBanner");
    updateBanner->setStyleSheet("QPushButton { min-height: 64px; background: #f4dfad; color: #47330c; border: 2px solid #937023; font-weight: bold; } QPushButton:hover { background: #eed095; } QPushButton:focus { border: 3px solid #47330c; }");
    updateBanner->setAccessibleDescription("Download the available update, or install it when the download is ready.");
    updateBanner->hide(); layout->insertWidget(1, updateBanner);
    connect(updates, &UpdateWidget::updateAvailable, updateBanner, &QWidget::setVisible);
    connect(updates, &UpdateWidget::bannerTextChanged, updateBanner, &QPushButton::setText);
    connect(updates, &UpdateWidget::installing, this, &Window::pause);
    connect(updates, &UpdateWidget::installerOpened, qApp, &QApplication::quit);
    for (const auto &link : QList<QPair<QString, QString>>{
             {"Visit the Stable Mouse website", "https://hunter-boone.github.io/stable-mouse/"},
             {"Open the source code and report an issue", "https://github.com/Hunter-Boone/stable-mouse"}}) {
        auto *button = new QPushButton(link.first); button->setAccessibleDescription("Opens " + link.second + " in your web browser.");
        connect(button, &QPushButton::clicked, this, [url = link.second] { QDesktopServices::openUrl(QUrl(url)); });
        aboutLayout->addWidget(button);
    }
    const QList<QPair<QString, QString>> faq{
        {"Is it really free?", "Yes. Stable Mouse is free, open-source software under GPL-3.0-only. There is no subscription, account, advertising, or paid tier. You can inspect, modify, and share the code under its license."},
        {"Can it help with hand tremor?", "Stable Mouse is being built for people who find mouse control difficult because of unwanted hand movement. It reduces some kinds of repeated shaking in generated tests, but usefulness varies. Those results do not establish whether it will help a particular person or condition."},
        {"Will the pointer feel delayed?", "Yes. Smoothing adds delay, especially at higher strengths. Center tracking can improve steadiness, but can also add delay when you move toward a new target. Try lower smoothing if the pointer feels too slow."},
        {"How do I turn it off?", "Use Turn Stability Off in the app or tray, or Quit Stable Mouse. On Windows, press Ctrl + Alt + F8. On macOS, press Control + Option + F8. On Linux, hold the left and right mouse buttons together for two seconds. Escape turns stability off while the app window has focus."},
        {"Does it record my movement?", "No. The app does not save movement history or collect analytics. Your preferences stay on your computer. The website adds no analytics, cookies, or third-party scripts. GitHub hosts the website and downloads under its own privacy policy."},
        {"Does it work in every app?", "It targets ordinary desktop pointer use. Raw-input games, protected administrator windows, remote connections, touchpads, and specialized devices can behave differently or bypass filtering. The repository explains platform support and current limitations."},
        {"What do the movement methods do?", "Smoothing only softens all movement. Recognize shaking looks for back-and-forth movement and follows its center. Always follow the center uses the center of recent movement continuously. The smoothing amount applies to every method."},
        {"What does my operating system need?", backend->instructions()},
        {"What happens when I close the window?", "Closing the window keeps Stable Mouse in the tray if your desktop supports it. The Quit Stable Mouse button lets you choose to minimize to the tray or quit. Quitting turns stability off. Turn off Open when I sign in before uninstalling."},
        {"What do the startup options do?", "Open when I sign in starts the app when you sign in to your computer. Turn Mouse Stability on when opened also enables your selected movement method and smoothing automatically. Leave the second option off if you prefer to turn stability on yourself."}
    };
    for (const auto &entry : faq) {
        auto *card = new QGroupBox(entry.first); auto *cardLayout = new QVBoxLayout(card);
        cardLayout->addWidget(copy(entry.second)); aboutLayout->addWidget(card);
    }
    aboutLayout->setSpacing(16); aboutLayout->addStretch();
    auto *helpScroll = new QScrollArea; helpScroll->setObjectName("helpScroll");
    helpScroll->setWidgetResizable(true); helpScroll->setFrameShape(QFrame::NoFrame); helpScroll->setWidget(about);
    tabs->addTab(helpScroll, "Help");
    connect(updateBanner, &QPushButton::clicked, this, [tabs, helpScroll, updatesGroup, updates] {
        tabs->setCurrentIndex(2);
        updates->activateUpdate();
        helpScroll->ensureWidgetVisible(updatesGroup);
    });

    auto *quit = new QPushButton("Quit Stable Mouse"); quit->setObjectName("quit"); quit->setMinimumHeight(52); layout->addWidget(quit);
    setCentralWidget(root);
    connect(quit, &QPushButton::clicked, this, &Window::chooseExit);
    connect(toggleButton, &QPushButton::clicked, this, &Window::toggle);
    connect(strength, &QSlider::valueChanged, this, &Window::updateConfig);
    connect(speed, &QSlider::valueChanged, this, &Window::updateConfig);
    connect(centerMethod, &QButtonGroup::idToggled, this, [this](int, bool checked) { if (checked) updateConfig(); });
    connect(centerWindow, &QSlider::valueChanged, this, &Window::updateConfig);
    connect(enabledOnLaunch, &QCheckBox::toggled, this, [this](bool on) { settings.setValue("enableOnLaunch", on); });
    connect(login, &QCheckBox::toggled, this, [this, testMode](bool on) {
        if (testMode) return;
        QString error;
        if (!Startup::setEnabled(on, &error)) { QSignalBlocker block(login); login->setChecked(Startup::enabled()); showProblem(error); }
    });
    connect(backend.get(), &Backend::stateChanged, this, [this] { starting = false; syncState(); });
    connect(backend.get(), &Backend::problem, this, &Window::showProblem);
    connect(backend.get(), &Backend::emergencyPause, this, [this] { pause(); notice->setText("Stability turned off with the emergency control."); notice->show(); reveal(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this); connect(escape, &QShortcut::activated, this, &Window::pause);
    if (!testMode && QSystemTrayIcon::isSystemTrayAvailable()) {
        tray = new QSystemTrayIcon(windowIcon(), this); auto *menu = new QMenu(this);
        menu->addAction("Open Stable Mouse", this, &Window::reveal);
        trayToggle = menu->addAction("Turn Stability On", this, &Window::toggle);
        menu->addSeparator(); menu->addAction("Quit", qApp, &QApplication::quit);
        tray->setContextMenu(menu); tray->show();
        connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) { if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) reveal(); });
    }
    updateConfig(); syncState();
    if (!testMode && enabledOnLaunch->isChecked()) QTimer::singleShot(300, this, [this] { if (!backend->active() && !starting) toggle(); });
    if (!startupLaunch || !tray) showMaximized();
}
Window::~Window() { backend->stop(); }
void Window::chooseExit() {
    if (auto *existing = findChild<QDialog *>("exitDialog")) { existing->raise(); return; }
    auto *dialog = new QDialog(this);
    dialog->setObjectName("exitDialog"); dialog->setWindowTitle("Close Stable Mouse");
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setModal(true);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(16);
    layout->addWidget(copy("Keep Stable Mouse running or quit?"));
    layout->addWidget(copy("Minimizing keeps your current stability setting. Quitting turns stability off."));
    auto *minimize = new QPushButton("Minimize to system tray"); minimize->setObjectName("exitMinimize");
    minimize->setEnabled(tray != nullptr); layout->addWidget(minimize);
    if (!tray) layout->addWidget(copy("The system tray is not available on this desktop."));
    auto *quit = new QPushButton("Quit application"); quit->setObjectName("exitQuit"); layout->addWidget(quit);
    auto *cancel = new QPushButton("Cancel"); cancel->setObjectName("exitCancel"); layout->addWidget(cancel);
    for (auto *button : {minimize, quit, cancel}) { button->setMinimumHeight(56); button->setAutoDefault(false); }
    cancel->setDefault(true);
    connect(cancel, &QPushButton::clicked, dialog, &QDialog::reject);
    connect(minimize, &QPushButton::clicked, this, [this, dialog] { dialog->accept(); hide(); });
    connect(quit, &QPushButton::clicked, this, [this, dialog] { pause(); dialog->accept(); qApp->quit(); });
    dialog->resize(520, dialog->sizeHint().height()); dialog->open(); cancel->setFocus();
}
void Window::reveal() { showMaximized(); raise(); activateWindow(); }
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
    status->setText(on ? "Stability is on" : starting ? "Waiting for mouse access…" : "Stability is off · Your mouse moves normally");
    toggleButton->setText(on ? "Turn Stability Off" : starting ? "Cancel" : "Turn Stability On");
    if (device) device->setEnabled(!on && !starting);
    if (tray) { tray->setToolTip(on ? "Stable Mouse: Stability on" : "Stable Mouse: Stability off"); trayToggle->setText(toggleButton->text()); }
}
void Window::updateConfig() {
    strengthValue->setText(QString("Smoothing strength: %1%").arg(strength->value()));
    speedValue->setText(QString("Pointer speed: %1%").arg(speed->value()));
    settings.setValue("strength", strength->value()); settings.setValue("speed", speed->value());
    const int mode = centerMethod->checkedId();
    settings.setValue("centerTracking", mode != 0); settings.setValue("centerMode", mode);
    settings.setValue("centerWindow", centerWindow->value());
    centerWindowValue->setText(QString("Center window: %1 ms").arg(centerWindow->value()));
    centerWindowControls->setVisible(mode == 2);
    centerHelp->setText(mode == 0 ? "Softens all mouse movement." : mode == 1 ? "Looks for back-and-forth shaking before following its center." : "Follows the center of recent movement, then smooths it. This can add delay.");
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
