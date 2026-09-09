// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QPalette>
#include <QString>

namespace Appearance {
bool systemIsDark();
QPalette palette(bool dark);
QString styleSheet(bool dark);
}
