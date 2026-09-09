// SPDX-License-Identifier: GPL-3.0-only
#include "update_widget.h"
#include "updater.h"
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#endif

namespace {
bool openInstaller(const QString &path) {
#ifdef Q_OS_WIN
    // ShellExecute supports the installer's UAC prompt. NSIS requires /D last,
    // without quotes even when the destination contains spaces.
    QDir installRoot(QCoreApplication::applicationDirPath());
    QString arguments;
    if (installRoot.dirName().compare("bin", Qt::CaseInsensitive) == 0) {
        installRoot.cdUp(); arguments = "/D=" + QDir::toNativeSeparators(installRoot.absolutePath());
    }
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open",
        reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()),
        arguments.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(arguments.utf16()), nullptr, SW_SHOWNORMAL)) > 32;
#else
    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
}
}

UpdateWidget::UpdateWidget(bool testMode, QWidget *parent, Updater *service,
                           std::function<bool(const QString &)> launcher) : QWidget(parent) {
    setObjectName("updates");
    updater = service ? service : new Updater(this);
    if (!launcher) launcher = openInstaller;
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(12);
    auto *automatic = new QCheckBox("Check for updates automatically"); automatic->setObjectName("automaticUpdates");
    automatic->setChecked(QSettings().value("updates/automatic", true).toBool()); layout->addWidget(automatic);
    auto *privacy = new QLabel("Checks GitHub at startup and once a day, including preview releases for preview builds. No settings or mouse movement are sent. Downloads and installation start only when you choose.");
    privacy->setWordWrap(true); layout->addWidget(privacy);
    auto *status = new QLabel; status->setObjectName("updateStatus"); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); layout->addWidget(status);
    auto *progress = new QProgressBar; progress->setObjectName("updateProgress"); progress->setRange(0, 100); layout->addWidget(progress);
    auto *check = new QPushButton("Check for updates"); check->setObjectName("checkUpdates"); layout->addWidget(check);
    auto *download = new QPushButton("Download update"); download->setObjectName("downloadUpdate"); layout->addWidget(download);
    auto *install = installButton = new QPushButton("Install update and close Stable Mouse"); install->setObjectName("installUpdate"); layout->addWidget(install);
    auto *instructions = new QLabel;
#ifdef Q_OS_MACOS
    instructions->setText("The disk image will open. Drag Stable Mouse into Applications, replace the old copy, then reopen the app.");
#elif defined(Q_OS_WIN)
    instructions->setText("Follow the installer prompts, then reopen Stable Mouse. Your settings will be kept.");
#else
    instructions->setText("The package will open in your software installer. Install it, then reopen Stable Mouse. Your settings will be kept.");
#endif
    instructions->setWordWrap(true); layout->addWidget(instructions);
    auto *notes = notesButton = new QPushButton("View release notes and downloads"); notes->setObjectName("updateNotes"); layout->addWidget(notes);
    auto *cancel = new QPushButton("Cancel update"); cancel->setObjectName("cancelUpdate"); layout->addWidget(cancel);
    const auto refresh = [=] {
        const auto state = updater->state();
        const bool busy = state == Updater::State::Checking || state == Updater::State::Downloading;
        status->setText(updater->message());
        check->setEnabled(!busy); cancel->setVisible(busy);
        download->setVisible(state == Updater::State::Available && !updater->release().download.isEmpty());
        install->setVisible(state == Updater::State::Ready); instructions->setVisible(state == Updater::State::Ready);
        notes->setVisible(!updater->release().page.isEmpty());
        progress->setVisible(state == Updater::State::Downloading);
        if (state != Updater::State::Downloading) progress->setValue(0);
        QString action = "Download update";
        if (state == Updater::State::Downloading) action = "Downloading… View progress";
        else if (state == Updater::State::Ready) action = "Install update";
        else if (state == Updater::State::Error) action = "Update failed. Retry";
        else if (updater->release().download.isEmpty()) action = "View update downloads";
        emit bannerTextChanged("Stable Mouse " + updater->release().version + " available · " + action);
        emit updateAvailable(!updater->release().version.isEmpty());
    };
    connect(updater, &Updater::changed, this, refresh); refresh();
    connect(updater, &Updater::progress, this, [progress](qint64 received, qint64 total) { progress->setValue(int(received * 100 / total)); });
    connect(check, &QPushButton::clicked, updater, &Updater::check);
    connect(download, &QPushButton::clicked, updater, &Updater::download);
    connect(cancel, &QPushButton::clicked, updater, &Updater::cancel);
    connect(notes, &QPushButton::clicked, this, [this, status] {
        if (!QDesktopServices::openUrl(updater->release().page)) status->setText("Could not open your browser. Visit github.com/Hunter-Boone/stable-mouse/releases for downloads.");
    });
    connect(install, &QPushButton::clicked, this, [this, launcher, status] {
        if (!updater->verifyInstaller()) return;
        emit installing();
        if (!launcher(updater->installerPath())) {
            status->setText("Could not open the installer. Stability is off. Try again, or open the downloaded file: " + updater->installerPath()); return;
        }
        updater->retainInstaller(); emit installerOpened();
    });
    const auto automaticCheck = [=](bool startup) {
        if (testMode || !automatic->isChecked()) return;
        if (updater->state() != Updater::State::Idle && updater->state() != Updater::State::Current && updater->state() != Updater::State::Error) return;
        const auto now = QDateTime::currentDateTimeUtc();
        const auto last = QSettings().value("updates/lastAutomaticCheck").toDateTime();
        if (!startup && last.isValid() && last <= now && last.secsTo(now) < 24 * 60 * 60) return;
        QSettings().setValue("updates/lastAutomaticCheck", now); updater->check();
    };
    connect(automatic, &QCheckBox::toggled, this, [automaticCheck](bool checked) {
        QSettings().setValue("updates/automatic", checked); if (checked) automaticCheck(true);
    });
    if (!testMode) {
        QTimer::singleShot(2000, this, [automaticCheck] { automaticCheck(true); });
        auto *timer = new QTimer(this); timer->setObjectName("automaticUpdateTimer"); timer->setInterval(60 * 60 * 1000);
        connect(timer, &QTimer::timeout, this, [automaticCheck] { automaticCheck(false); }); timer->start();
    }
}

void UpdateWidget::activateUpdate() {
    switch (updater->state()) {
    case Updater::State::Available:
        if (updater->release().download.isEmpty()) notesButton->click();
        else updater->download();
        break;
    case Updater::State::Ready: installButton->click(); break;
    case Updater::State::Error: updater->check(); break;
    default: break;
    }
}
