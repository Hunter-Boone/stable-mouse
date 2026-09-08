// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QUrl>
#include <QPointer>
#include <QFile>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

struct UpdateRelease {
    QString version, fileName;
    QUrl page, download;
    QByteArray sha256;
    qint64 size = 0;
};

namespace Updates {
// Returns false for invalid versions as well as older/equal versions.
bool newer(const QString &candidate, const QString &installed);
QString platformSuffix(const QString &os, const QString &osVersion, const QString &architecture);
UpdateRelease selectRelease(const QByteArray &json, const QString &installed,
                            const QString &suffix, QString *error);
}

class Updater final : public QObject {
    Q_OBJECT
public:
    enum class State { Idle, Checking, Current, Available, Downloading, Ready, Error };
    explicit Updater(QObject *parent = nullptr, QNetworkAccessManager *network = nullptr);
    ~Updater() override;
    State state() const { return currentState; }
    QString message() const { return status; }
    UpdateRelease release() const { return available; }
    QString installerPath() const;
    void check();
    void download();
    void cancel();
    bool verifyInstaller();
    void retainInstaller();
signals:
    void changed();
    void progress(qint64 received, qint64 total);
private:
    void setState(State state, const QString &message);
    QNetworkReply *get(const QUrl &url);
    QNetworkAccessManager *network;
    QPointer<QNetworkReply> reply;
    State currentState = State::Idle;
    QString status = "Check for a newer version of Stable Mouse.";
    UpdateRelease available;
    QByteArray metadata;
    std::unique_ptr<QTemporaryDir> directory;
    QFile file;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    qint64 received = 0;
};
