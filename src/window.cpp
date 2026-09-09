// SPDX-License-Identifier: GPL-3.0-only
#include "window.h"
#include "appearance.h"
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
#include <QFrame>
#include <QUrl>
#include <QScrollArea>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStyleHints>
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
QLabel *copy(const QString &text, const char *role = nullptr) {
    auto *label = new QLabel(text); label->setWordWrap(true); label->setTextFormat(Qt::PlainText);
    if (role) label->setObjectName(role);
    return label;
}
// Every button declares what kind of action it is, so the stylesheet can give
// each kind one consistent look instead of a page of identical rectangles.
QPushButton *action(const QString &text, const char *kind, const char *name = nullptr) {
    auto *button = new QPushButton(text); button->setProperty("kind", kind);
    if (name) button->setObjectName(name);
    return button;
}
// Reading width is capped so controls stay a comfortable size on wide or
// maximized windows instead of stretching into thin full-width strips.
QWidget *column(QWidget *content, int maxWidth = 960) {
    auto *host = new QWidget; host->setObjectName("column");
    auto *row = new QHBoxLayout(host); row->setContentsMargins(0, 0, 0, 0); row->setSpacing(0);
    content->setMaximumWidth(maxWidth);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // The content takes all the width it may; only the remainder is split
    // between the two margins, so the column grows to the cap before centering.
    row->addStretch(0); row->addWidget(content, 1); row->addStretch(0);
    return host;
}
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
    applyAppearance();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (settings.value("appearance", "system").toString() == "system") applyAppearance();
    });
#endif
    auto *page = new QWidget; auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(16);
    auto *root = column(page, 1040);
    auto *title = copy("Stable Mouse", "title"); layout->addWidget(title);
    status = copy("Paused", "status"); layout->addWidget(status);
    toggleButton = action("Turn Stability On", "primary", "toggle");
    toggleButton->setAccessibleDescription("Turns system mouse stabilization on or off."); layout->addWidget(toggleButton);
    layout->addWidget(copy(backend->escapeHint(), "hint"));
    notice = copy(""); notice->setObjectName("notice"); notice->hide(); layout->addWidget(notice);
    auto *tabs = new QTabWidget; layout->addWidget(tabs, 1);
    auto *controls = new QWidget; controls->setObjectName("settingsPage"); auto *controlsLayout = new QVBoxLayout(controls); controlsLayout->setSpacing(24); controlsLayout->setContentsMargins(4, 20, 16, 20);
    auto *smoothingGroup = new QGroupBox("How much smoothing?"); auto *smoothingLayout = new QVBoxLayout(smoothingGroup);
    auto *presets = new QHBoxLayout; presets->setSpacing(12);
    auto *presetButtons = new QButtonGroup(this); presetButtons->setExclusive(true); presetButtons->setObjectName("presetButtons");
    const QList<QPair<QString, int>> values{{"Light", 25}, {"Balanced", 55}, {"Strong", 85}};
    for (const auto &preset : values) {
        auto *button = action(preset.first, "choice"); button->setObjectName(QString("preset%1").arg(preset.second));
        button->setCheckable(true); presetButtons->addButton(button);
        button->setAccessibleName(preset.first + " smoothing preset"); presets->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, value = preset.second] { strength->setValue(value); });
    }
    smoothingLayout->setSpacing(12); smoothingLayout->addLayout(presets);
    smoothingLayout->addWidget(copy("More smoothing is steadier, but takes longer to follow your hand.", "hint"));
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
    centerHelp = copy("", "hint"); methodsLayout->addWidget(centerHelp);
    strengthValue = copy(""); smoothingLayout->addWidget(strengthValue);
    strength = new ScrollSafeSlider(Qt::Horizontal); strength->setObjectName("strength"); strength->setRange(0, 100); strength->setPageStep(10);
    strength->setAccessibleName("Smoothing strength"); strength->setValue(std::clamp(settings.value("strength", 55).toInt(), 0, 100));
    smoothingLayout->addWidget(strength);
    centerWindowControls = new QWidget; auto *windowLayout = new QVBoxLayout(centerWindowControls); windowLayout->setContentsMargins(0,0,0,0);
    centerWindowValue = copy(""); windowLayout->addWidget(centerWindowValue);
    centerWindow = new ScrollSafeSlider(Qt::Horizontal); centerWindow->setObjectName("centerWindow"); centerWindow->setRange(100,600); centerWindow->setSingleStep(25); centerWindow->setPageStep(50);
    centerWindow->setAccessibleName("Center window, milliseconds"); centerWindow->setValue(std::clamp(settings.value("centerWindow",250).toInt(),100,600));
    windowLayout->addWidget(centerWindow); windowLayout->addWidget(copy("A longer window uses more recent movement to find the center. It can feel steadier, but slower.", "hint"));
    methodsLayout->addWidget(centerWindowControls);
    auto *speedGroup = new QGroupBox("Pointer speed"); speedGroup->setObjectName("pointerSpeedGroup");
    auto *speedLayout = new QVBoxLayout(speedGroup); controlsLayout->addWidget(speedGroup);
    speedValue = copy(""); speedLayout->addWidget(speedValue);
    speed = new ScrollSafeSlider(Qt::Horizontal); speed->setObjectName("speed"); speed->setRange(25, 200); speed->setPageStep(10);
    speed->setAccessibleName("Pointer speed, percent"); speed->setValue(std::clamp(settings.value("speed", 100).toInt(), 25, 200)); speedLayout->addWidget(speed);
    speedLayout->addWidget(copy("Pointer speed is separate from smoothing. Leave it at 100% to start.", "hint"));
#ifdef Q_OS_LINUX
    auto *devices = new QGroupBox("Mouse to stabilize"); auto *deviceLayout = new QVBoxLayout(devices);
    device = new ScrollSafeComboBox; device->setAccessibleName("Mouse to stabilize");
    auto *deviceRow = new QHBoxLayout; deviceRow->setSpacing(12); deviceRow->addWidget(device, 1); deviceLayout->addLayout(deviceRow);
    auto *refresh = action("Refresh", "secondary"); refresh->setAccessibleName("Refresh mice"); deviceRow->addWidget(refresh);
    connect(refresh, &QPushButton::clicked, this, &Window::refreshDevices);
    controlsLayout->insertWidget(0, devices); refreshDevices();
#endif
    auto *appearanceGroup = new QGroupBox("Appearance");
    auto *appearanceLayout = new QVBoxLayout(appearanceGroup);
    auto *appearance = new ScrollSafeComboBox; appearance->setObjectName("appearance");
    appearance->setAccessibleName("Color theme");
    appearance->addItem("Use system theme", "system");
    appearance->addItem("Light", "light"); appearance->addItem("Dark", "dark");
    const auto savedAppearance = settings.value("appearance", "system").toString();
    appearance->setCurrentIndex(std::max(0, appearance->findData(savedAppearance)));
    appearanceLayout->addWidget(appearance);
    controlsLayout->insertWidget(0, appearanceGroup);
    connect(appearance, &QComboBox::currentIndexChanged, this, [this, appearance] {
        settings.setValue("appearance", appearance->currentData()); applyAppearance();
    });
    auto *startup = new QGroupBox("When you sign in"); auto *startupLayout = new QVBoxLayout(startup);
    login = new RowCheckBox("Open when I sign in"); login->setObjectName("login"); login->setChecked(testMode ? false : Startup::enabled()); startupLayout->addWidget(login);
    auto *enabledOnLaunch = new RowCheckBox("Turn Mouse Stability on when opened"); enabledOnLaunch->setObjectName("enableOnLaunch");
    enabledOnLaunch->setChecked(settings.value("enableOnLaunch", false).toBool()); startupLayout->addWidget(enabledOnLaunch);
    controlsLayout->addWidget(startup); controlsLayout->addStretch();
    auto *scroll = new QScrollArea; scroll->setObjectName("settingsScroll"); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidget(controls); tabs->addTab(scroll, "Settings");
    auto *practicePage = new QWidget; practicePage->setObjectName("practicePage"); auto *practiceLayout = new QVBoxLayout(practicePage);
    practiceLayout->setContentsMargins(4, 20, 4, 8); practiceLayout->setSpacing(16);
    practiceLayout->addWidget(copy("Move between the large targets and click each one. Compare how it feels with Stability on and off. Nothing here is recorded or saved.", "hint"));
    auto *practice = new Practice; practiceLayout->addWidget(practice, 1);
    auto *clear = action("Clear practice area", "secondary"); practiceLayout->addWidget(clear); connect(clear, &QPushButton::clicked, practice, &Practice::clear);
    tabs->addTab(practicePage, "Practice");
    auto *about = new QWidget; about->setObjectName("helpPage"); auto *aboutLayout = new QVBoxLayout(about);
    aboutLayout->setContentsMargins(4, 20, 16, 20); aboutLayout->setSpacing(16);
    aboutLayout->addWidget(copy("Stable Mouse " + QCoreApplication::applicationVersion() + " • Preview", "heading"));
    aboutLayout->addWidget(copy("Free software, licensed under GPL-3.0-only. No account, advertising, analytics, or movement history.", "hint"));
    auto *updatesGroup = new QGroupBox("Updates"); auto *updatesLayout = new QVBoxLayout(updatesGroup);
    auto *updates = new UpdateWidget(testMode); updatesLayout->addWidget(updates); aboutLayout->addWidget(updatesGroup);
    auto *updateBanner = new QPushButton("An update is available · Download update");
    updateBanner->setObjectName("updateBanner");
    updateBanner->setAccessibleDescription("Download the available update, or install it when the download is ready.");
    updateBanner->hide(); layout->insertWidget(1, updateBanner);
    connect(updates, &UpdateWidget::updateAvailable, updateBanner, &QWidget::setVisible);
    connect(updates, &UpdateWidget::bannerTextChanged, updateBanner, &QPushButton::setText);
    connect(updates, &UpdateWidget::installing, this, &Window::pause);
    connect(updates, &UpdateWidget::installerOpened, qApp, &QApplication::quit);
    auto *linksGroup = new QGroupBox("On the web"); auto *linksLayout = new QVBoxLayout(linksGroup); linksLayout->setSpacing(4);
    for (const auto &link : QList<QPair<QString, QString>>{
             {"Visit the Stable Mouse website", "https://hunter-boone.github.io/stable-mouse/"},
             {"Open the source code and report an issue", "https://github.com/Hunter-Boone/stable-mouse"}}) {
        auto *button = action(link.first + "  ↗", "link"); button->setAccessibleName(link.first);
        button->setAccessibleDescription("Opens " + link.second + " in your web browser.");
        connect(button, &QPushButton::clicked, this, [url = link.second] { QDesktopServices::openUrl(QUrl(url)); });
        linksLayout->addWidget(button);
    }
    aboutLayout->addWidget(linksGroup);
    aboutLayout->addWidget(copy("Common questions", "heading"));
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
        auto *item = new QFrame; item->setObjectName("faq"); auto *itemLayout = new QVBoxLayout(item);
        itemLayout->setContentsMargins(20, 6, 8, 6); itemLayout->setSpacing(6);
        itemLayout->addWidget(copy(entry.first, "question")); itemLayout->addWidget(copy(entry.second, "answer"));
        aboutLayout->addWidget(item);
    }
    aboutLayout->addStretch();
    auto *helpScroll = new QScrollArea; helpScroll->setObjectName("helpScroll");
    helpScroll->setWidgetResizable(true); helpScroll->setFrameShape(QFrame::NoFrame); helpScroll->setWidget(about);
    tabs->addTab(helpScroll, "Help");
    connect(updateBanner, &QPushButton::clicked, this, [tabs, helpScroll, updatesGroup, updates] {
        tabs->setCurrentIndex(2);
        updates->activateUpdate();
        helpScroll->ensureWidgetVisible(updatesGroup);
    });

    auto *quit = action("Quit Stable Mouse", "quiet", "quit");
    auto *footer = new QHBoxLayout; footer->addStretch(1); footer->addWidget(quit, 0); layout->addLayout(footer);
    quit->setMinimumWidth(280);
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
    applyAppearance(); updateConfig(); syncState();
    if (!testMode && enabledOnLaunch->isChecked()) QTimer::singleShot(300, this, [this] { if (!backend->active() && !starting) toggle(); });
    if (!startupLaunch || !tray) showMaximized();
}
Window::~Window() { backend->stop(); }
void Window::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ApplicationPaletteChange && settings.value("appearance", "system").toString() == "system")
        applyAppearance();
}
void Window::applyAppearance() {
    const auto choice = settings.value("appearance", "system").toString();
    const bool dark = choice == "dark" || (choice != "light" && Appearance::systemIsDark());
    setStyleSheet(Appearance::styleSheet(dark));
    // Apply the palette after replacing the stylesheet so Qt does not resolve
    // the new palette through the previous theme's text colors.
    setPalette(Appearance::palette(dark));
    // Custom painting reads palette roles that stylesheet inheritance does not
    // carry to the practice canvas. Give it the complete theme explicitly.
    for (auto *practice : findChildren<Practice *>()) practice->setPalette(Appearance::palette(dark));
}

void Window::chooseExit() {
    if (auto *existing = findChild<QDialog *>("exitDialog")) { existing->raise(); return; }
    auto *dialog = new QDialog(this);
    dialog->setObjectName("exitDialog"); dialog->setWindowTitle("Close Stable Mouse");
    dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->setModal(true);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 24, 24, 24); layout->setSpacing(16);
    layout->addWidget(copy("Keep Stable Mouse running or quit?", "heading"));
    layout->addWidget(copy("Minimizing keeps your current stability setting. Quitting turns stability off.", "hint"));
    auto *minimize = action("Minimize to system tray", "primary", "exitMinimize");
    minimize->setEnabled(tray != nullptr); layout->addWidget(minimize);
    if (!tray) layout->addWidget(copy("The system tray is not available on this desktop.", "hint"));
    auto *quit = action("Quit application", "secondary", "exitQuit"); layout->addWidget(quit);
    auto *cancel = action("Cancel", "quiet", "exitCancel"); layout->addWidget(cancel);
    for (auto *button : {minimize, quit, cancel}) button->setAutoDefault(false);
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
    status->setProperty("active", on); status->style()->unpolish(status); status->style()->polish(status);
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
