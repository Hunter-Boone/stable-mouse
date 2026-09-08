// SPDX-License-Identifier: GPL-3.0-only
#include "window.h"
#include <QApplication>
#include <QDir>
#include <QCryptographicHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    const bool smoke = app.arguments().contains("--smoke-test");
    const int screenshotIndex = app.arguments().indexOf("--screenshot");
    const bool testMode = smoke || screenshotIndex >= 0;
    QCoreApplication::setOrganizationName(testMode ? "StableMouseTests" : "StableMouse");
    QCoreApplication::setApplicationName("Stable Mouse");
    QCoreApplication::setApplicationVersion(QStringLiteral(STABLE_MOUSE_VERSION));
    QStandardPaths::setTestModeEnabled(testMode);
    const auto dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataPath);
    QLockFile lock(dataPath + "/instance.lock");
    QLocalServer server;
    const auto socketName = "stable-mouse-" + QString::fromLatin1(QCryptographicHash::hash(dataPath.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    if (!testMode) {
        if (!lock.tryLock(100)) {
            QLocalSocket socket; socket.connectToServer(socketName);
            if (socket.waitForConnected(1000)) { socket.write("SHOW\n"); socket.waitForBytesWritten(1000); return 0; }
            QMessageBox::information(nullptr, "Stable Mouse", "Stable Mouse is already running. Open it from the system tray."); return 0;
        }
        QLocalServer::removeServer(socketName);
        server.setSocketOptions(QLocalServer::UserAccessOption);
        server.listen(socketName);
    }
    Window window(createBackend(), app.arguments().contains("--background"), testMode);
    QObject::connect(&server, &QLocalServer::newConnection, &window, [&] {
        while (auto *socket = server.nextPendingConnection()) { window.reveal(); socket->disconnectFromServer(); socket->deleteLater(); }
    });
    if (smoke) QTimer::singleShot(150, &app, [&] { std::cout << "Stable Mouse window opened without activating input.\n"; app.quit(); });
    if (screenshotIndex >= 0) QTimer::singleShot(300, &app, [&] {
        const auto path = app.arguments().value(screenshotIndex + 1);
        const bool saved = !path.isEmpty() && window.grab().save(path);
        app.exit(saved ? 0 : 1);
    });
    return app.exec();
}
