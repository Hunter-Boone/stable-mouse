// SPDX-License-Identifier: GPL-3.0-only
#include "appearance.h"
#include <QApplication>
#include <QMap>
#include <QStyleHints>

namespace {
QMap<QString, QString> colors(bool dark) {
    return {
        {"window", dark ? "#121a26" : "#f5f7fa"},
        {"surface", dark ? "#1d2938" : "#ffffff"},
        {"text", dark ? "#edf3fb" : "#18283d"},
        {"muted", dark ? "#b4c2d3" : "#52647a"},
        {"border", dark ? "#8194ac" : "#74869c"},
        {"soft", dark ? "#2a394c" : "#e6edf6"},
        {"teal", dark ? "#69d8bf" : "#166b63"},
        {"tealHover", dark ? "#91e5d2" : "#0c554e"},
        {"tealSoft", dark ? "#183e38" : "#e2f3ef"},
        {"tealText", dark ? "#a4ead9" : "#0e594f"},
        {"indicator", dark ? "#246d62" : "#166b63"},
        {"accent", dark ? "#aac3ff" : "#3459bc"},
        {"accentHover", dark ? "#c5d6ff" : "#24439a"},
        {"accentSoft", dark ? "#263b62" : "#e8edfc"},
        {"accentText", dark ? "#d8e5ff" : "#233f8c"},
        {"onColor", dark ? "#101c2c" : "#ffffff"},
        {"focus", dark ? "#ffbe75" : "#9e3c00"},
        {"notice", dark ? "#47380f" : "#fff0c5"},
        {"noticeText", dark ? "#ffe2a4" : "#513900"},
        {"noticeBorder", dark ? "#bf9648" : "#947020"},
        {"disabled", dark ? "#24303f" : "#e6e9ef"},
        {"disabledText", dark ? "#9eacbf" : "#667387"},
        {"chevron", dark ? ":/icons/chevron-light.xpm" : ":/icons/chevron.xpm"}
    };
}
}

bool Appearance::systemIsDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto scheme = qApp->styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown) return scheme == Qt::ColorScheme::Dark;
#endif
    return qApp->palette().color(QPalette::Window).lightness() < 128;
}

QPalette Appearance::palette(bool dark) {
    const auto c = colors(dark);
    QPalette result;
    for (const auto role : {QPalette::Window, QPalette::Base, QPalette::AlternateBase, QPalette::Button,
                           QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::Highlight,
                           QPalette::HighlightedText, QPalette::ToolTipBase, QPalette::ToolTipText,
                           QPalette::Light, QPalette::Midlight, QPalette::Mid, QPalette::Dark,
                           QPalette::Shadow, QPalette::Link, QPalette::LinkVisited, QPalette::PlaceholderText}) {
        QString key = "text";
        switch (role) {
        case QPalette::Window: key = "window"; break;
        case QPalette::Base: case QPalette::Button: case QPalette::ToolTipBase: key = "surface"; break;
        case QPalette::AlternateBase: case QPalette::Midlight: key = "soft"; break;
        case QPalette::Highlight: case QPalette::Link: case QPalette::LinkVisited: key = "accent"; break;
        case QPalette::HighlightedText: key = "onColor"; break;
        case QPalette::Mid: case QPalette::Dark: key = "border"; break;
        case QPalette::Shadow: key = "window"; break;
        case QPalette::Light: key = "surface"; break;
        case QPalette::PlaceholderText: key = "muted"; break;
        default: break;
        }
        result.setColor(role, QColor(c[key]));
    }
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        result.setColor(QPalette::Disabled, role, QColor(c["disabledText"]));
    result.setColor(QPalette::Disabled, QPalette::Button, QColor(c["disabled"]));
    return result;
}

QString Appearance::styleSheet(bool dark) {
    QString sheet = R"(
        QWidget { font-size: 12pt; color: @text@; }
        QMainWindow, QDialog, QMenu, QScrollArea, QTabWidget::pane, QWidget#settingsPage, QWidget#helpPage { background: @window@; }
        QLabel { color: @text@; background: transparent; }
        QLabel#title { font-size: 25pt; font-weight: bold; }
        QLabel#status { background: @soft@; color: @muted@; padding: 12px; border-radius: 8px; font-weight: bold; }
        QLabel#status[active="true"] { background: @tealSoft@; color: @tealText@; }
        QLabel#notice { background: @notice@; color: @noticeText@; padding: 12px; border-radius: 8px; }
        QPushButton, QComboBox { min-height: 44px; padding: 4px 14px; border: 2px solid @border@; border-radius: 7px; background: @surface@; color: @text@; }
        QPushButton:hover, QComboBox:hover { background: @accentSoft@; border-color: @accent@; }
        QComboBox::drop-down { width: 30px; border: none; }
        QComboBox::down-arrow { image: url(@chevron@); width: 14px; height: 9px; }
        QComboBox QAbstractItemView { background: @surface@; color: @text@; selection-background-color: @accent@; selection-color: @onColor@; border: 2px solid @border@; }
        QPushButton:checked { background: @accent@; color: @onColor@; border-color: @accent@; font-weight: bold; }
        QPushButton:checked:hover { background: @accentHover@; }
        QPushButton#toggle { background: @teal@; color: @onColor@; border-color: @teal@; font-weight: bold; }
        QPushButton#toggle:hover { background: @tealHover@; }
        QPushButton:focus, QComboBox:focus, QCheckBox:focus, QRadioButton:focus { border: 3px solid @focus@; }
        QGroupBox { background: @surface@; border: 1px solid @border@; border-radius: 10px; margin-top: 12px; padding: 18px 12px 12px; }
        QGroupBox::title { subcontrol-origin: margin; left: 16px; color: @accentText@; font-weight: bold; }
        QCheckBox, QRadioButton { min-height: 64px; padding: 8px 16px; spacing: 16px; color: @text@; border: 3px solid @border@; border-radius: 8px; background: @surface@; }
        QCheckBox:hover, QRadioButton:hover { background: @accentSoft@; border-color: @accent@; }
        QCheckBox:checked, QRadioButton:checked { background: @tealSoft@; border-color: @teal@; color: @tealText@; }
        QRadioButton::indicator { width: 28px; height: 28px; border: 2px solid @border@; border-radius: 16px; background: @surface@; }
        QRadioButton::indicator:checked { background: @indicator@; border-color: @teal@; image: url(:/icons/check.xpm); }
        QCheckBox::indicator { width: 28px; height: 28px; border: 2px solid @border@; border-radius: 4px; background: @surface@; }
        QCheckBox::indicator:checked { background: @indicator@; border-color: @teal@; image: url(:/icons/check.xpm); }
        QSlider { min-height: 44px; background: transparent; }
        QSlider::groove:horizontal { height: 8px; background: @soft@; border: 1px solid @border@; border-radius: 4px; }
        QSlider::sub-page:horizontal { background: @accent@; border-radius: 4px; }
        QSlider::handle:horizontal { width: 28px; margin: -11px 0; background: @accent@; border: 2px solid @surface@; border-radius: 15px; }
        QSlider::handle:horizontal:focus { border: 3px solid @focus@; }
        QTabWidget::pane { border: none; border-top: 1px solid @border@; }
        QTabBar::tab { min-height: 40px; padding: 4px 20px; color: @muted@; border-bottom: 3px solid transparent; }
        QTabBar::tab:selected { color: @accentText@; background: @accentSoft@; border-bottom: 3px solid @accent@; font-weight: bold; }
        QTabBar::tab:focus { border: 2px solid @focus@; }
        QScrollBar:vertical { background: @soft@; width: 20px; margin: 0; }
        QScrollBar::handle:vertical { background: @border@; min-height: 48px; border: 4px solid @soft@; border-radius: 9px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QProgressBar { min-height: 28px; border: 1px solid @border@; border-radius: 5px; background: @surface@; color: @text@; text-align: center; }
        QProgressBar::chunk { background: @tealSoft@; }
        QMenu::item:selected { background: @accent@; color: @onColor@; }
        QPushButton#updateBanner { min-height: 64px; background: @notice@; color: @noticeText@; border: 2px solid @noticeBorder@; font-weight: bold; }
        QPushButton#updateBanner:hover, QPushButton#updateBanner:focus { border: 3px solid @focus@; }
        QPushButton:disabled, QComboBox:disabled { background: @disabled@; color: @disabledText@; border-color: @border@; }
        QLabel:disabled, QCheckBox:disabled, QRadioButton:disabled { color: @disabledText@; }
    )";
    const auto c = colors(dark);
    for (auto it = c.begin(); it != c.end(); ++it) sheet.replace("@" + it.key() + "@", it.value());
    return sheet;
}
