// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace Startup {
bool enabled();
bool setEnabled(bool enable, QString *error);
QString entryContents(const QString &executable);
}
