// SPDX-License-Identifier: GPL-3.0-only
#include "updater.h"
#include "update_widget.h"
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QtTest>
#include <cstring>

namespace {
const QByteArray installer = "test installer bytes\n";
const QString releaseRoot = "https://github.com/Hunter-Boone/stable-mouse/releases/";
QString suffix() { return Updates::platformSuffix(QSysInfo::productType(), QSysInfo::productVersion(), QSysInfo::buildCpuArchitecture()); }
QJsonObject release(const QString &tag = "v0.1.1-preview.1", const QString &platform = suffix()) {
    const QString name = "Stable-Mouse-0.1.1-" + platform;
    return {{"tag_name", tag}, {"html_url", releaseRoot + "tag/" + tag}, {"prerelease", tag.contains('-')},
        {"assets", QJsonArray{QJsonObject{{"name", name}, {"size", installer.size()},
            {"digest", "sha256:" + QString::fromLatin1(QCryptographicHash::hash(installer, QCryptographicHash::Sha256).toHex())},
            {"browser_download_url", releaseRoot + "download/" + tag + "/" + name}}}}};
}
QByteArray feed(const QJsonObject &entry = release()) { return QJsonDocument(QJsonArray{entry}).toJson(); }

// Exercise the real asynchronous updater with deterministic network replies.
class Reply final : public QNetworkReply {
public:
    Reply(const QNetworkRequest &request, QByteArray bytes, NetworkError failure, bool hang, QObject *parent)
        : QNetworkReply(parent), data(std::move(bytes)) {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly);
        if (!hang) QTimer::singleShot(0, this, [this, failure] {
            if (isFinished()) return;
            if (failure != NoError) setError(failure, "Test network failure");
            emit readyRead();
            if (!isFinished()) { setFinished(true); emit finished(); }
        });
    }
    void abort() override { if (!isFinished()) { setError(OperationCanceledError, "Cancelled"); setFinished(true); emit finished(); } }
    qint64 bytesAvailable() const override { return data.size() - offset + QNetworkReply::bytesAvailable(); }
    bool isSequential() const override { return true; }
protected:
    qint64 readData(char *buffer, qint64 maximum) override {
        const qint64 size = qMin(maximum, qint64(data.size()) - offset);
        if (!size) return -1;
        std::memcpy(buffer, data.constData() + offset, size); offset += size; return size;
    }
private:
    QByteArray data; qint64 offset = 0;
};
class Network final : public QNetworkAccessManager {
public:
    QByteArray metadata = feed(), payload = installer;
    QNetworkReply::NetworkError failure = QNetworkReply::NoError;
    bool hang = false;
    QList<QNetworkRequest> requests;
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        requests.append(request);
        return new Reply(request, request.url().host() == "api.github.com" ? metadata : payload, failure, hang, this);
    }
};
}

class UpdaterTests final : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName("StableMouseUpdaterTests-" + QString::number(QCoreApplication::applicationPid()));
        QCoreApplication::setApplicationName("UpdaterTests");
    }
    void init() { QCoreApplication::setApplicationVersion("0.1.0-preview.1"); QSettings().clear(); }
    void versionOrdering() {
        QVERIFY(Updates::newer("v0.1.10-preview.1", "0.1.9-preview.20"));
        QVERIFY(Updates::newer("0.1.0-preview.10", "0.1.0-preview.2"));
        QVERIFY(Updates::newer("0.1.0", "0.1.0-preview.99"));
        QVERIFY(!Updates::newer("0.1.0-preview.1", "0.1.0"));
        QVERIFY(!Updates::newer("v0.1.0+build2", "0.1.0+build1"));
        QVERIFY(!Updates::newer("garbage", "0.1.0"));
        QVERIFY(!Updates::newer("0.2.0-preview.01", "0.1.0"));
        QVERIFY(!Updates::newer("1.0.0", "unknown"));
    }
    void platformSelection() {
        QCOMPARE(Updates::platformSuffix("windows", "10", "x86_64"), "Windows-x64.exe");
        QCOMPARE(Updates::platformSuffix("osx", "14", "arm64"), "macOS-universal.dmg");
        QCOMPARE(Updates::platformSuffix("debian", "12.8", "x86_64"), "Debian12-amd64.deb");
        QCOMPARE(Updates::platformSuffix("ubuntu", "24.04", "x86_64"), "Ubuntu24.04-amd64.deb");
        QVERIFY(Updates::platformSuffix("ubuntu", "22.04", "x86_64").isEmpty());
        QVERIFY(Updates::platformSuffix("windows", "11", "arm64").isEmpty());
        QVERIFY(Updates::platformSuffix("debian", "12", "arm64").isEmpty());
    }
    void releaseChannelsAndTrust() {
        QString error;
        const auto parse = [&error](const QByteArray &data, const QString &installed = "0.1.0-preview.1") {
            return Updates::selectRelease(data, installed, "Windows-x64.exe", &error);
        };
        auto entry = release("v0.1.1-preview.1", "Windows-x64.exe");
        QVERIFY(!parse(feed(entry)).download.isEmpty()); QVERIFY(error.isEmpty());
        QVERIFY(parse(feed(entry), "0.1.0").version.isEmpty());
        QVERIFY(parse(feed(entry), "0.1.1-preview.1").version.isEmpty());
        auto draft = entry; draft["draft"] = true; QVERIFY(parse(feed(draft)).version.isEmpty());
        QJsonArray unordered{release("v0.1.10", "Windows-x64.exe"), entry, release("v0.1.2", "Windows-x64.exe")};
        QCOMPARE(parse(QJsonDocument(unordered).toJson()).version, "v0.1.10");
        auto badPage = entry; badPage["html_url"] = "https://example.com/installer";
        QVERIFY(parse(feed(badPage)).version.isEmpty());
        for (const QString &field : {QString("digest"), QString("browser_download_url"), QString("name"), QString("size")}) {
            auto altered = entry; auto asset = altered["assets"].toArray()[0].toObject();
            asset[field] = field == "size" ? QJsonValue(-1) : QJsonValue("../../untrusted.exe");
            altered["assets"] = QJsonArray{asset};
            const auto result = parse(feed(altered));
            QVERIFY(!result.version.isEmpty()); QVERIFY(result.download.isEmpty());
        }
        auto duplicate = entry; auto assets = entry["assets"].toArray(); assets.append(assets[0]); duplicate["assets"] = assets;
        QVERIFY(parse(feed(duplicate)).download.isEmpty());
        QVERIFY(Updates::selectRelease(feed(entry), "0.1.0-preview.1", "", &error).download.isEmpty());
        parse("not json"); QVERIFY(!error.isEmpty());
        parse("{}"); QVERIFY(!error.isEmpty());
        QVERIFY(parse("[]").version.isEmpty()); QVERIFY(error.isEmpty());
    }
    void downloadVerifyAndCleanup() {
        if (suffix().isEmpty()) QSKIP("No packaged updater target on this test host");
        QString path;
        {
            Network network; Updater updater(nullptr, &network); QSignalSpy progress(&updater, &Updater::progress);
            updater.check(); updater.check(); QCOMPARE(network.requests.size(), 1);
            QTRY_COMPARE(updater.state(), Updater::State::Available);
            QVERIFY(network.requests[0].url().toString().contains("per_page=100"));
            QCOMPARE(network.requests[0].transferTimeout(), 30000);
            updater.download(); updater.download(); QCOMPARE(network.requests.size(), 2);
            QTRY_COMPARE(updater.state(), Updater::State::Ready);
            path = updater.installerPath(); QFile downloaded(path); QVERIFY(downloaded.open(QIODevice::ReadOnly));
            QCOMPARE(downloaded.readAll(), installer); downloaded.close();
            QVERIFY(updater.verifyInstaller()); QVERIFY(!progress.isEmpty());
            QFile changed(path); QVERIFY(changed.open(QIODevice::WriteOnly)); changed.write("changed"); changed.close();
            QVERIFY(!updater.verifyInstaller()); QCOMPARE(updater.state(), Updater::State::Error);
        }
        QVERIFY(!QFile::exists(path));
    }
    void failedDownloads_data() {
        QTest::addColumn<QByteArray>("payload");
        QTest::newRow("truncated") << installer.left(5);
        QTest::newRow("wrong checksum") << QByteArray(installer.size(), 'x');
        QTest::newRow("too large") << installer + "excess";
    }
    void failedDownloads() {
        if (suffix().isEmpty()) QSKIP("No packaged updater target on this test host");
        QFETCH(QByteArray, payload);
        Network network; network.payload = payload; Updater updater(nullptr, &network);
        updater.check(); QTRY_COMPARE(updater.state(), Updater::State::Available);
        updater.download(); QTRY_COMPARE(updater.state(), Updater::State::Error);
        QVERIFY(updater.installerPath().isEmpty());
        network.payload = installer;
        updater.check(); QTRY_COMPARE(updater.state(), Updater::State::Available);
        updater.download(); QTRY_COMPARE(updater.state(), Updater::State::Ready);
    }
    void cancellationAndNetworkFailure() {
        Network network; Updater updater(nullptr, &network);
        network.hang = true; updater.check(); QCOMPARE(updater.state(), Updater::State::Checking);
        updater.cancel(); QCOMPARE(updater.state(), Updater::State::Idle);
        network.hang = false; network.failure = QNetworkReply::ContentAccessDenied;
        updater.check(); QTRY_COMPARE(updater.state(), Updater::State::Error);
        network.failure = QNetworkReply::NoError;
        updater.check(); QTRY_COMPARE(updater.state(), Updater::State::Available);
        if (!suffix().isEmpty()) {
            network.hang = true; updater.download(); QCOMPARE(updater.state(), Updater::State::Downloading);
            updater.cancel(); QCOMPARE(updater.state(), Updater::State::Available);
            QVERIFY(updater.installerPath().isEmpty());
        }
    }
    void uiInstallerHandoff() {
        if (suffix().isEmpty()) QSKIP("No packaged updater target on this test host");
        Network network; Updater updater(nullptr, &network);
        bool paused = false, accept = false; QString opened;
        UpdateWidget widget(true, nullptr, &updater, [&](const QString &path) {
            if (!paused) return false;
            opened = path; return accept;
        });
        connect(&widget, &UpdateWidget::installing, &widget, [&] { paused = true; });
        QSignalSpy finished(&widget, &UpdateWidget::installerOpened);
        auto *automatic = widget.findChild<QCheckBox *>("automaticUpdates");
        automatic->setChecked(false); QVERIFY(!QSettings().value("updates/automatic").toBool());
        automatic->setChecked(true); QVERIFY(network.requests.isEmpty()); // Test mode stays offline.
        widget.findChild<QPushButton *>("checkUpdates")->click();
        QTRY_COMPARE(updater.state(), Updater::State::Available);
        widget.findChild<QPushButton *>("downloadUpdate")->click();
        QTRY_COMPARE(updater.state(), Updater::State::Ready);
        widget.findChild<QPushButton *>("installUpdate")->click();
        QVERIFY(paused); QVERIFY(!opened.isEmpty()); QCOMPARE(finished.size(), 0);
        QVERIFY(widget.findChild<QLabel *>("updateStatus")->text().contains("Could not open"));
        accept = true; widget.findChild<QPushButton *>("installUpdate")->click();
        QCOMPARE(finished.size(), 1); QVERIFY(QFile::exists(opened));
        QDir(QFileInfo(opened).absolutePath()).removeRecursively();
    }
    void automaticChecksRespectPreferenceAndDailyLimit() {
        Network network; network.metadata = "[]";
        {
            Updater updater(nullptr, &network); UpdateWidget widget(false, nullptr, &updater);
            QTRY_COMPARE_WITH_TIMEOUT(network.requests.size(), 1, 4000);
            QTRY_COMPARE(updater.state(), Updater::State::Current);
            QVERIFY(QSettings().value("updates/lastAutomaticCheck").toDateTime().isValid());
        }
        {
            Updater updater(nullptr, &network); UpdateWidget widget(false, nullptr, &updater);
            QTest::qWait(2200); QCOMPARE(network.requests.size(), 1);
            // Explicit checks bypass the automatic daily limit.
            widget.findChild<QPushButton *>("checkUpdates")->click();
            QTRY_COMPARE(updater.state(), Updater::State::Current); QCOMPARE(network.requests.size(), 2);
        }
        QSettings().remove("updates/lastAutomaticCheck"); QSettings().setValue("updates/automatic", false);
        {
            Updater updater(nullptr, &network); UpdateWidget widget(false, nullptr, &updater);
            QTest::qWait(2200); QCOMPARE(network.requests.size(), 2);
            widget.findChild<QCheckBox *>("automaticUpdates")->setChecked(true);
            QTRY_COMPARE(updater.state(), Updater::State::Current); QCOMPARE(network.requests.size(), 3);
        }
    }
    void livePublishedRelease() {
        if (!qEnvironmentVariableIsSet("STABLE_MOUSE_TEST_LIVE_UPDATER")) QSKIP("Opt-in read-only GitHub integration check");
        if (suffix().isEmpty()) QSKIP("No packaged updater target on this test host");
        Updater updater;
        updater.check(); QTRY_COMPARE_WITH_TIMEOUT(updater.state(), Updater::State::Available, 45000);
        QVERIFY(!updater.release().download.isEmpty());
        updater.download(); QTRY_COMPARE_WITH_TIMEOUT(updater.state(), Updater::State::Ready, 120000);
        QVERIFY(updater.verifyInstaller());
        qInfo() << "Verified published installer" << updater.release().version << updater.release().fileName << updater.release().size;
    }
};
QTEST_MAIN(UpdaterTests)
#include "updater_tests.moc"
