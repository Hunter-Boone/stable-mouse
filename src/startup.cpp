// SPDX-License-Identifier: GPL-3.0-only
#include "startup.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QXmlStreamWriter>

namespace {
QString path() {
#ifdef Q_OS_MACOS
    return QDir::homePath() + "/Library/LaunchAgents/org.stablemouse.StableMouse.plist";
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/autostart/org.stablemouse.StableMouse.desktop";
#endif
}
}
QString Startup::entryContents(const QString &executable) {
#ifdef Q_OS_MACOS
    QString xml;
    QXmlStreamWriter w(&xml);
    w.setAutoFormatting(true); w.writeStartDocument();
    w.writeDTD("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
    w.writeStartElement("plist"); w.writeAttribute("version", "1.0"); w.writeStartElement("dict");
    w.writeTextElement("key", "Label"); w.writeTextElement("string", "org.stablemouse.StableMouse");
    w.writeTextElement("key", "ProgramArguments"); w.writeStartElement("array");
    w.writeTextElement("string", executable); w.writeTextElement("string", "--background"); w.writeEndElement();
    w.writeTextElement("key", "RunAtLoad"); w.writeEmptyElement("true");
    w.writeEndElement(); w.writeEndElement(); w.writeEndDocument(); return xml;
#elif defined(Q_OS_WIN)
    return "\"" + QDir::toNativeSeparators(executable) + "\" --background";
#else
    QString quoted = executable;
    quoted.replace("\\", "\\\\").replace("\"", "\\\"").replace("`", "\\`").replace("$", "\\$").replace("%", "%%");
    // Desktop string escaping is applied after Exec argument escaping.
    quoted.replace("\\", "\\\\");
    return "[Desktop Entry]\nType=Application\nName=Stable Mouse\nExec=\"" + quoted + "\" --background\nTerminal=false\n";
#endif
}
bool Startup::enabled() {
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return reg.contains("StableMouse");
#else
    return QFile::exists(path());
#endif
}
bool Startup::setEnabled(bool enable, QString *error) {
#ifdef Q_OS_WIN
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enable) reg.setValue("StableMouse", entryContents(QCoreApplication::applicationFilePath()));
    else reg.remove("StableMouse");
    reg.sync();
    if (reg.status() != QSettings::NoError) { *error = "Windows could not save the login setting."; return false; }
#else
    const auto filePath = path();
    if (!enable) {
        if (QFile::exists(filePath) && !QFile::remove(filePath)) { *error = "Could not remove the login entry."; return false; }
        return true;
    }
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) { *error = "Could not create the login folder."; return false; }
    QSaveFile file(filePath);
    const auto contents = entryContents(QCoreApplication::applicationFilePath()).toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit()) {
        *error = "Could not save the login entry: " + file.errorString(); return false;
    }
#endif
    return true;
}
