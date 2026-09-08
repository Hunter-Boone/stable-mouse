// SPDX-License-Identifier: GPL-3.0-only
#include "updater.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>

namespace {
const QString repository = "https://github.com/Hunter-Boone/stable-mouse/releases/";
constexpr qint64 maxInstallerSize = 512 * 1024 * 1024;
struct Version { QStringList core, pre; bool valid = false; };
Version version(QString text) {
    static const QRegularExpression syntax(
        "^v?(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)"
        "(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?(?:\\+[0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*)?$");
    const auto match = syntax.match(text);
    if (!match.hasMatch()) return {};
    Version result{{match.captured(1), match.captured(2), match.captured(3)}, {}, true};
    if (!match.captured(4).isEmpty()) result.pre = match.captured(4).split('.');
    static const QRegularExpression numeric("^[0-9]+$");
    for (const auto &part : result.pre)
        if (numeric.match(part).hasMatch() && part.size() > 1 && part.startsWith('0')) result.valid = false;
    return result;
}
int numberCompare(const QString &a, const QString &b) {
    return a.size() != b.size() ? (a.size() > b.size() ? 1 : -1) : QString::compare(a, b);
}
int compare(const Version &a, const Version &b) {
    for (int i = 0; i < 3; ++i) { const int c = numberCompare(a.core[i], b.core[i]); if (c) return c; }
    if (a.pre.isEmpty() != b.pre.isEmpty()) return a.pre.isEmpty() ? 1 : -1;
    static const QRegularExpression numeric("^[0-9]+$");
    for (qsizetype i = 0; i < qMin(a.pre.size(), b.pre.size()); ++i) {
        const bool an = numeric.match(a.pre[i]).hasMatch(), bn = numeric.match(b.pre[i]).hasMatch();
        const int c = an != bn ? (an ? -1 : 1) : an ? numberCompare(a.pre[i], b.pre[i]) : QString::compare(a.pre[i], b.pre[i]);
        if (c) return c;
    }
    return a.pre.size() == b.pre.size() ? 0 : a.pre.size() > b.pre.size() ? 1 : -1;
}
bool trusted(const QUrl &url, const QString &prefix) {
    return url.isValid() && url.scheme() == "https" && url.host() == "github.com"
        && url.userInfo().isEmpty() && url.port() == -1 && !url.hasQuery() && !url.hasFragment()
        && url.toString(QUrl::FullyEncoded).startsWith(prefix);
}
}

bool Updates::newer(const QString &candidate, const QString &installed) {
    const auto a = version(candidate), b = version(installed);
    return a.valid && b.valid && compare(a, b) > 0;
}
QString Updates::platformSuffix(const QString &os, const QString &osVersion, const QString &architecture) {
    if (os == "windows" && architecture == "x86_64") return "Windows-x64.exe";
    if (os == "osx" && (architecture == "x86_64" || architecture == "arm64")) return "macOS-universal.dmg";
    if (architecture == "x86_64") {
        if (os == "debian" && osVersion.section('.', 0, 0) == "12") return "Debian12-amd64.deb";
        if (os == "ubuntu" && osVersion == "24.04") return "Ubuntu24.04-amd64.deb";
    }
    return {};
}
UpdateRelease Updates::selectRelease(const QByteArray &json, const QString &installed,
                                     const QString &suffix, QString *error) {
    error->clear();
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray() || !version(installed).valid) {
        *error = "The update information could not be read. Try again later."; return {};
    }
    UpdateRelease best;
    for (const auto &entry : doc.array()) {
        const auto obj = entry.toObject();
        const QString tag = obj.value("tag_name").toString();
        if (obj.value("draft").toBool() || !newer(tag, installed) || (!best.version.isEmpty() && !newer(tag, best.version))) continue;
        // Preview builds follow previews; stable builds stay on stable releases.
        if (version(installed).pre.isEmpty() && (obj.value("prerelease").toBool() || !version(tag).pre.isEmpty())) continue;
        const QUrl page(obj.value("html_url").toString());
        if (!trusted(page, repository + "tag/")) continue;
        best = {}; best.version = tag; best.page = page;
        if (suffix.isEmpty()) continue;
        static const QRegularExpression digestPattern("^sha256:([0-9a-fA-F]{64})$");
        const QRegularExpression namePattern("^stable-mouse-[0-9A-Za-z.+-]+-" + QRegularExpression::escape(suffix) + "$", QRegularExpression::CaseInsensitiveOption);
        for (const auto &value : obj.value("assets").toArray()) {
            const auto asset = value.toObject();
            const auto name = asset.value("name").toString();
            const auto digest = digestPattern.match(asset.value("digest").toString());
            const QUrl url(asset.value("browser_download_url").toString());
            const qint64 size = asset.value("size").toInteger();
            if (!namePattern.match(name).hasMatch() || !digest.hasMatch() || size <= 0 || size > maxInstallerSize
                || !trusted(url, repository + "download/" + tag + "/") || url.fileName() != name) continue;
            if (!best.download.isEmpty()) { best.download = QUrl(); best.fileName.clear(); break; } // Ambiguous package: use the release page.
            best.fileName = name; best.download = url; best.sha256 = QByteArray::fromHex(digest.captured(1).toLatin1()); best.size = size;
        }
    }
    return best;
}

Updater::Updater(QObject *parent, QNetworkAccessManager *manager)
    : QObject(parent), network(manager ? manager : new QNetworkAccessManager(this)) {}
Updater::~Updater() {
    if (reply) { reply->disconnect(this); reply->abort(); reply->deleteLater(); }
    file.close();
}
void Updater::setState(State state, const QString &message) { currentState = state; status = message; emit changed(); }
QNetworkReply *Updater::get(const QUrl &url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "Stable-Mouse-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(30000);
    return network->get(request);
}
void Updater::check() {
    if (currentState == State::Checking || currentState == State::Downloading) return;
    file.close(); directory.reset(); available = {}; metadata.clear();
    setState(State::Checking, "Checking for updates...");
    reply = get(QUrl("https://api.github.com/repos/Hunter-Boone/stable-mouse/releases?per_page=100"));
    connect(reply, &QNetworkReply::readyRead, this, [this] {
        metadata += reply->readAll();
        if (metadata.size() > 2 * 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this] {
        auto *finished = reply.data(); reply = nullptr; finished->deleteLater();
        metadata += finished->readAll();
        if (finished->error() != QNetworkReply::NoError || metadata.size() > 2 * 1024 * 1024) {
            setState(State::Error, "Could not check for updates. Check your connection and try again later."); return;
        }
        QString error;
        available = Updates::selectRelease(metadata, QCoreApplication::applicationVersion(),
            Updates::platformSuffix(QSysInfo::productType(), QSysInfo::productVersion(), QSysInfo::buildCpuArchitecture()), &error);
        if (!error.isEmpty()) { setState(State::Error, error); return; }
        if (available.version.isEmpty()) setState(State::Current, "You have the latest version for your release channel.");
        else setState(State::Available, "Stable Mouse " + available.version + " is available."
            + (available.download.isEmpty() ? " Open the release page for download options for your system." : ""));
    });
}
void Updater::download() {
    if (currentState != State::Available || available.download.isEmpty()) return;
    const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/updates";
    if (!QDir().mkpath(cache)) { setState(State::Error, "Could not create the update download folder."); return; }
    directory = std::make_unique<QTemporaryDir>(cache + "/download-XXXXXX");
    file.setFileName(directory->filePath(available.fileName));
    if (!directory->isValid() || !file.open(QIODevice::WriteOnly)) {
        directory.reset(); setState(State::Error, "Could not save the installer. Check available disk space and try again."); return;
    }
    received = 0; hash.reset();
    setState(State::Downloading, "Downloading the update...");
    reply = get(available.download);
    reply->setReadBufferSize(256 * 1024);
    connect(reply, &QNetworkReply::readyRead, this, [this] {
        const auto bytes = reply->readAll(); received += bytes.size();
        if (received > available.size || file.write(bytes) != bytes.size()) { reply->abort(); return; }
        hash.addData(bytes); emit progress(received, available.size);
    });
    connect(reply, &QNetworkReply::finished, this, [this] {
        auto *finished = reply.data(); reply = nullptr; finished->deleteLater();
        const bool valid = finished->error() == QNetworkReply::NoError && received == available.size
            && hash.result() == available.sha256 && file.flush();
        file.close();
        if (!valid) { directory.reset(); setState(State::Error, "The download failed or did not match its checksum. Check for updates to try again."); return; }
        setState(State::Ready, "The update is ready. Installing will turn stability off and close Stable Mouse.");
    });
}
void Updater::cancel() {
    if (!reply) return;
    reply->disconnect(this); reply->abort(); reply->deleteLater(); reply = nullptr;
    file.close(); directory.reset();
    setState(available.version.isEmpty() ? State::Idle : State::Available, "Update cancelled. You can try again when ready.");
}
QString Updater::installerPath() const { return currentState == State::Ready ? file.fileName() : QString(); }
bool Updater::verifyInstaller() {
    QFile installer(installerPath()); QCryptographicHash verification(QCryptographicHash::Sha256);
    if (currentState == State::Ready && installer.open(QIODevice::ReadOnly) && installer.size() == available.size
        && verification.addData(&installer) && verification.result() == available.sha256) return true;
    installer.close(); directory.reset(); setState(State::Error, "The installer changed or is missing. Check for updates to download it again."); return false;
}
void Updater::retainInstaller() { if (directory) directory->setAutoRemove(false); }
