// SPDX-License-Identifier: GPL-3.0-only
#include "window.h"
#include "appearance.h"
#include "practice.h"
#include "startup.h"
#include "update_widget.h"
#include <QCheckBox>
#include <QDialog>
#include <QComboBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QtTest>
#include <cmath>

class FakeBackend final : public Backend {
public:
    bool allowStart = true;
    FilterConfig config;
    bool start(const QString &) override { if (!allowStart) { emit problem("Device unavailable"); return false; } setActive(true); return true; }
    void stop() override { setActive(false); }
    void configure(FilterConfig c) override { config = c; }
    QString instructions() const override { return "Test backend"; }
    QString escapeHint() const override { return "Esc pauses"; }
    QStringList devices() const override { return {"Test mouse"}; }
    void emergency() { stop(); emit emergencyPause(); }
};
class AppTests final : public QObject {
    Q_OBJECT
private slots:
    void appearanceChangesAndPersists() {
        QTemporaryDir configDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configDir.path());
        QCoreApplication::setOrganizationName("StableMouseTests"); QCoreApplication::setApplicationName("Appearance");
        const auto luminance = [](QColor color) {
            const auto linear = [](double value) { return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4); };
            return .2126 * linear(color.redF()) + .7152 * linear(color.greenF()) + .0722 * linear(color.blueF());
        };
        {
            Window window(std::make_unique<FakeBackend>(), false, true);
            window.showNormal(); window.resize(900, 1000);
            auto *choice = window.findChild<QComboBox *>("appearance"); QVERIFY(choice);
            auto *tabs = window.findChild<QTabWidget *>();
            window.findChild<QSlider *>("strength")->setValue(55);
            for (const auto &mode : {QString("light"), QString("dark")}) {
                choice->setCurrentIndex(choice->findData(mode)); QCoreApplication::processEvents();
                QCOMPARE(QSettings().value("appearance").toString(), mode);
                QCOMPARE(window.findChild<QSlider *>("strength")->value(), 55);
                const auto palette = window.palette();
                QCOMPARE(palette.color(QPalette::Window).lightness() < 128, mode == "dark");
                QCOMPARE(window.grab().toImage().pixelColor(5, 5), palette.color(QPalette::Window));
                tabs->setCurrentIndex(1); QCoreApplication::processEvents();
                auto *practice = window.findChild<Practice *>(); QVERIFY(practice);
                QCOMPARE(practice->grab().toImage().pixelColor(0, 0), Appearance::palette(mode == "dark").color(QPalette::Base));
                tabs->setCurrentIndex(0);
                for (const auto pair : {qMakePair(QPalette::Text, QPalette::Base), qMakePair(QPalette::WindowText, QPalette::Window), qMakePair(QPalette::HighlightedText, QPalette::Highlight)}) {
                    const auto a = luminance(palette.color(pair.first)), b = luminance(palette.color(pair.second));
                    QVERIFY2((std::max(a, b) + .05) / (std::min(a, b) + .05) >= 4.5,
                        qPrintable(mode + ": " + palette.color(pair.first).name() + " on " + palette.color(pair.second).name()));
                }
                if (qEnvironmentVariableIsSet("STABLE_MOUSE_THEME_CAPTURE")) {
                    const auto folder = qEnvironmentVariable("STABLE_MOUSE_THEME_CAPTURE"); QDir().mkpath(folder);
                    for (int tab = 0; tab < tabs->count(); ++tab) {
                        tabs->setCurrentIndex(tab); QCoreApplication::processEvents();
                        QVERIFY(window.grab().save(folder + "/" + mode + "-" + QString::number(tab) + ".png"));
                    }
                    window.findChild<QPushButton *>("quit")->click(); QCoreApplication::processEvents();
                    auto *dialog = window.findChild<QDialog *>("exitDialog"); QVERIFY(dialog);
                    QVERIFY(dialog->grab().save(folder + "/" + mode + "-dialog.png"));
                    dialog->reject(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                    tabs->setCurrentIndex(0);
                }
            }
        }
        Window reopened(std::make_unique<FakeBackend>(), false, true);
        auto *choice = reopened.findChild<QComboBox *>("appearance");
        QCOMPARE(choice->currentData().toString(), QString("dark"));
        QVERIFY(reopened.palette().color(QPalette::Window).lightness() < 128);
        choice->setCurrentIndex(choice->findData("system"));
        QCOMPARE(QSettings().value("appearance").toString(), QString("system"));
        QCOMPARE(reopened.palette().color(QPalette::Window).lightness() < 128, Appearance::systemIsDark());
    }
    void updatesReleaseMouseAndOpenHelp() {
        auto fake = std::make_unique<FakeBackend>(); auto *input = fake.get();
        Window window(std::move(fake), false, true);
        auto *updates = window.findChild<UpdateWidget *>(); QVERIFY(updates);
        auto *banner = window.findChild<QPushButton *>("updateBanner"); QVERIFY(banner->isHidden());
        emit updates->updateAvailable(true); QVERIFY(!banner->isHidden());
        emit updates->bannerTextChanged("Stable Mouse v9.0.0 available · Download update");
        QCOMPARE(banner->text(), QString("Stable Mouse v9.0.0 available · Download update"));
        window.showNormal(); window.resize(700, 820); QCoreApplication::processEvents();
        QVERIFY(banner->height() >= 64);
        QVERIFY(banner->geometry().bottom() < window.findChild<QLabel *>("status")->geometry().top());
        banner->click(); QCOMPARE(window.findChild<QTabWidget *>()->currentIndex(), 2);
        window.findChild<QPushButton *>("toggle")->click(); QVERIFY(input->active());
        emit updates->installing(); QVERIFY(!input->active());
        QCOMPARE(window.findChild<QPushButton *>("toggle")->text(), QString("Turn Stability On"));
        if (qEnvironmentVariableIsSet("STABLE_MOUSE_UPDATE_CAPTURE")) {
            window.showNormal(); window.resize(1000, 1000); QCoreApplication::processEvents();
            QVERIFY(window.grab().save(qEnvironmentVariable("STABLE_MOUSE_UPDATE_CAPTURE")));
        }
    }
    void scrollingDoesNotChangeSettings() {
        auto fake = std::make_unique<FakeBackend>();
        Window window(std::move(fake), false, true);
        for (const auto &name : {"strength", "speed", "centerWindow"}) {
            auto *slider = window.findChild<QSlider *>(name);
            const int before = slider->value();
            slider->setFocus();
            QWheelEvent wheel(QPointF(20,20), slider->mapToGlobal(QPoint(20,20)), QPoint(), QPoint(0,-120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(slider, &wheel);
            QCOMPARE(slider->value(), before);

            slider->setValue(slider->minimum());
            QTest::keyClick(slider, Qt::Key_Right);
            QCOMPARE(slider->value(), slider->minimum() + slider->singleStep());
        }
        // A wheel over the actual slider must
        // actually scroll the surrounding page, not merely leave values alone.
        window.showNormal(); window.resize(700, 650);
        QCoreApplication::processEvents();
        auto *scroll = window.findChild<QScrollArea *>("settingsScroll");
        auto *strength = window.findChild<QSlider *>("strength");
        scroll->ensureWidgetVisible(strength);
        QCoreApplication::processEvents();
        const int scrollBefore = scroll->verticalScrollBar()->value();
        const QPoint global = strength->mapToGlobal(strength->rect().center());
        QWheelEvent pageWheel(strength->rect().center(), global, QPoint(), QPoint(0,-120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(strength, &pageWheel);
        QVERIFY(scroll->verticalScrollBar()->value() > scrollBefore);
        if (qEnvironmentVariableIsSet("STABLE_MOUSE_UI_CAPTURE"))
            QVERIFY(window.grab().save(qEnvironmentVariable("STABLE_MOUSE_UI_CAPTURE")));

    }
    void largeTargetsAndMaximizedLaunch() {
        auto fake = std::make_unique<FakeBackend>(); auto *input = fake.get();
        Window window(std::move(fake), false, true);
        QVERIFY(window.isMaximized());
        QCoreApplication::processEvents();
        for (const auto &name : {"login", "enableOnLaunch"}) {
            auto *box = window.findChild<QCheckBox *>(name);
            QVERIFY(box->height() >= 64);
            const bool before = box->isChecked();
            QTest::mouseClick(box, Qt::LeftButton, Qt::NoModifier, QPoint(box->width()-8, box->height()/2));
            QCOMPARE(box->isChecked(), !before);
            QTest::keyClick(box, Qt::Key_Space);
            QCOMPARE(box->isChecked(), before);
        }
        for (int mode : {2, 0, 1}) {
            auto *radio = window.findChild<QRadioButton *>(QString("method%1").arg(mode));
            QVERIFY(radio->height() >= 64);
            QTest::mouseClick(radio, Qt::LeftButton, Qt::NoModifier, QPoint(radio->width()-8, radio->height()/2));
            QCOMPARE(window.findChild<QButtonGroup *>("centerMethod")->checkedId(), mode);
            QCOMPARE(input->config.centerTracking, mode != 0);
            QCOMPARE(input->config.alwaysCenter, mode == 2);
        }
        QVERIFY(window.findChild<QScrollArea *>("helpScroll"));
        if (qEnvironmentVariableIsSet("STABLE_MOUSE_DESIGN_CAPTURE")) {
            const auto path = qEnvironmentVariable("STABLE_MOUSE_DESIGN_CAPTURE");
            window.showNormal(); window.resize(1440, 1080);
            window.findChild<QSlider *>("strength")->setValue(55);
            window.findChild<QSlider *>("speed")->setValue(100);
            auto *scroll = window.findChild<QScrollArea *>("settingsScroll");
            QCoreApplication::processEvents();
            scroll->verticalScrollBar()->setValue(0);
            QVERIFY(window.grab().save(path + "/settings.png"));
            QCoreApplication::processEvents();
            scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            QVERIFY(window.grab().save(path + "/startup.png"));
            window.findChild<QTabWidget *>()->setCurrentIndex(2);
            QCoreApplication::processEvents();
            QVERIFY(window.grab().save(path + "/help.png"));
        }
    }
    void quitOffersSafeChoices() {
        auto fake = std::make_unique<FakeBackend>(); auto *input = fake.get();
        Window window(std::move(fake), false, true);
        window.findChild<QPushButton *>("toggle")->click();
        QVERIFY(input->active());
        window.findChild<QPushButton *>("quit")->click();
        auto *dialog = window.findChild<QDialog *>("exitDialog");
        QVERIFY(dialog && dialog->isVisible());
        QVERIFY(input->active());
        QVERIFY(!dialog->findChild<QPushButton *>("exitMinimize")->isEnabled());
        QVERIFY(dialog->findChild<QPushButton *>("exitCancel")->isDefault());
        dialog->findChild<QPushButton *>("exitCancel")->click();
        QVERIFY(window.isVisible()); QVERIFY(input->active());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        window.findChild<QPushButton *>("quit")->click();
        window.findChild<QPushButton *>("exitQuit")->click();
        QVERIFY(!input->active());
    }
    void controlsAndPersistence() {
        QTemporaryDir configDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configDir.path());
        QCoreApplication::setOrganizationName("StableMouseTests"); QCoreApplication::setApplicationName("Controls");
        {
            auto fake = std::make_unique<FakeBackend>(); auto *input = fake.get();
            Window window(std::move(fake), false, true);
            auto *toggle = window.findChild<QPushButton *>("toggle");
            QVERIFY(!input->active());
            QVERIFY(!input->config.centerTracking);
            window.findChild<QRadioButton *>("method2")->click();
            QVERIFY(input->config.alwaysCenter);
            window.findChild<QSlider *>("centerWindow")->setValue(450);
            QCOMPARE(input->config.centerWindow,.45);
            QVERIFY(input->config.centerTracking);
            QTest::mouseClick(toggle, Qt::LeftButton); QVERIFY(input->active());
            auto *strength = window.findChild<QSlider *>("strength"); strength->setValue(80); QCOMPARE(input->config.strength, 80.0);
            auto *speed = window.findChild<QSlider *>("speed"); speed->setValue(60); QCOMPARE(input->config.speed, .6);
            input->emergency(); QVERIFY(!input->active()); QCOMPARE(toggle->text(), QString("Turn Stability On"));
            input->allowStart = false; QTest::mouseClick(toggle, Qt::LeftButton);
            QVERIFY(!input->active()); QCOMPARE(window.findChild<QLabel *>("notice")->text(), QString("Device unavailable"));
            window.findChild<QCheckBox *>("enableOnLaunch")->setChecked(true);
        }
        auto fake = std::make_unique<FakeBackend>(); auto *input = fake.get();
        Window restored(std::move(fake), false, true);
        QCOMPARE(input->config.strength, 80.0); QCOMPARE(input->config.speed, .6);
        QVERIFY(restored.findChild<QCheckBox *>("enableOnLaunch")->isChecked());
        QVERIFY(input->config.centerTracking); QVERIFY(input->config.alwaysCenter);
        QCOMPARE(input->config.centerWindow,.45);
        restored.findChild<QRadioButton *>("method0")->click();
        QVERIFY(!input->config.centerTracking); QVERIFY(!input->config.alwaysCenter);
        QVERIFY(!input->active()); // Test mode never takes control of the real pointer.
    }
    void legacySettings() {
        QTemporaryDir configDir;
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,configDir.path());
        QCoreApplication::setOrganizationName("StableMouseTests"); QCoreApplication::setApplicationName("Migration");
        QSettings old; old.setValue("centerTracking",true); old.setValue("strength",85); old.sync();
        auto fake=std::make_unique<FakeBackend>(); auto *input=fake.get(); Window window(std::move(fake),false,true);
        QVERIFY(input->config.centerTracking); QVERIFY(!input->config.alwaysCenter); QCOMPARE(input->config.strength,85.);
    }
    void startupEscaping() {
        const auto contents = Startup::entryContents("/some folder/Mouse & pointer");
        QVERIFY(contents.contains("--background"));
#ifdef Q_OS_MACOS
        QVERIFY(contents.contains("Mouse &amp; pointer"));
#else
        QVERIFY(contents.contains("\""));
#endif
#ifdef Q_OS_LINUX
        const auto escaped = Startup::entryContents("/tmp/a%f$`\\\"mouse");
        QVERIFY(escaped.contains("%%f")); QVERIFY(escaped.contains("\\\\$"));
        QTemporaryDir dir;
        const auto previous = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", dir.path().toUtf8());
        QStandardPaths::setTestModeEnabled(false);
        QString error;
        QVERIFY2(Startup::setEnabled(true, &error), qPrintable(error)); QVERIFY(Startup::enabled());
        QVERIFY2(Startup::setEnabled(false, &error), qPrintable(error)); QVERIFY(!Startup::enabled());
        qputenv("XDG_CONFIG_HOME", previous);
#endif
    }
    void practiceRendersAndAcceptsClicks() {
        Practice practice; practice.resize(400, 300); practice.show();
        const auto before = practice.grab().toImage();
        QTest::mouseClick(&practice, Qt::LeftButton, Qt::NoModifier, QPoint(200, 150));
        QVERIFY(before != practice.grab().toImage());
        practice.clear(); QCOMPARE(before, practice.grab().toImage());
    }
};
QTEST_MAIN(AppTests)
#include "app_tests.moc"
