// SPDX-License-Identifier: GPL-3.0-only
#include "window.h"
#include "practice.h"
#include "startup.h"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

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
    void scrollingDoesNotChangeSettings() {
        auto fake = std::make_unique<FakeBackend>();
        Window window(std::move(fake), false, true);
        window.findChild<QPushButton *>("moreOptions")->click();
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
        window.resize(700, 650);
        QCoreApplication::processEvents();
        auto *scroll = window.findChild<QScrollArea *>();
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
        auto *method = window.findChild<QComboBox *>("centerMethod");
        method->setCurrentIndex(1); method->setFocus();
        QWheelEvent wheel(QPointF(20,20), method->mapToGlobal(QPoint(20,20)), QPoint(), QPoint(0,-120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(method, &wheel);
        QCOMPARE(method->currentIndex(), 1);

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
            QVERIFY(!window.findChild<QWidget *>("advancedOptions")->isVisible());
            window.findChild<QComboBox *>("centerMethod")->setCurrentIndex(2);
            QVERIFY(input->config.alwaysCenter);
            auto *more=window.findChild<QPushButton *>("moreOptions");
            QTest::mouseClick(more,Qt::LeftButton);
            QVERIFY(window.findChild<QWidget *>("advancedOptions")->isVisible());
            window.findChild<QSlider *>("centerWindow")->setValue(450);
            QCOMPARE(input->config.centerWindow,.45);
            QVERIFY(input->config.centerTracking);
            QTest::mouseClick(toggle, Qt::LeftButton); QVERIFY(input->active());
            auto *strength = window.findChild<QSlider *>("strength"); strength->setValue(80); QCOMPARE(input->config.strength, 80.0);
            auto *speed = window.findChild<QSlider *>("speed"); speed->setValue(60); QCOMPARE(input->config.speed, .6);
            input->emergency(); QVERIFY(!input->active()); QCOMPARE(toggle->text(), QString("Turn smoothing on"));
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
        QVERIFY(!restored.findChild<QWidget *>("advancedOptions")->isVisible());
        restored.findChild<QComboBox *>("centerMethod")->setCurrentIndex(0);
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
        QVERIFY(!window.findChild<QWidget *>("advancedOptions")->isVisible());
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
