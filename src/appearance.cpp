// SPDX-License-Identifier: GPL-3.0-only
#include "appearance.h"
#include <QApplication>
#include <QMap>
#include <QStyleHints>

namespace {
QMap<QString, QString> colors(bool dark) {
    return {
        {"window", dark ? "#121a26" : "#eef2f7"},
        {"surface", dark ? "#1d2938" : "#ffffff"},
        {"text", dark ? "#edf3fb" : "#18283d"},
        {"muted", dark ? "#b4c2d3" : "#52647a"},
        {"border", dark ? "#8194ac" : "#74869c"},
        {"cardBorder", dark ? "#34455a" : "#d3dbe6"},
        {"soft", dark ? "#2a394c" : "#e6edf6"},
        {"teal", dark ? "#69d8bf" : "#166b63"},
        {"tealHover", dark ? "#91e5d2" : "#0c554e"},
        {"tealDark", dark ? "#3bab92" : "#093f3a"},
        {"tealSoft", dark ? "#183e38" : "#e2f3ef"},
        {"tealText", dark ? "#a4ead9" : "#0e594f"},
        {"indicator", dark ? "#246d62" : "#166b63"},
        {"accent", dark ? "#aac3ff" : "#3459bc"},
        {"accentHover", dark ? "#c5d6ff" : "#24439a"},
        {"accentDark", dark ? "#7e9be6" : "#1d3680"},
        {"accentSoft", dark ? "#263b62" : "#e8edfc"},
        {"accentSoftHover", dark ? "#30498a" : "#d7e0fa"},
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

// Visual language, so every control reads as one kind of thing at a glance:
//  - Push buttons have a thick outline and bold text. kind="primary" is the
//    one filled action, "secondary" is a tinted helper, "link" opens something
//    elsewhere, "quiet" leaves or cancels, "choice" is one option in a set.
//  - Cards (QGroupBox) are flat surfaces with a soft border and a bold title.
//  - Questions (QFrame#faq) are plain text with a rule down the left side.
//  - Rows (checkbox / radio) are wide outlined tiles that fill when selected.
QString Appearance::styleSheet(bool dark) {
    QString sheet = R"(
        QWidget { font-size: 13pt; color: @text@; }
        QMainWindow, QDialog, QMenu, QScrollArea, QTabWidget::pane, QWidget#settingsPage, QWidget#helpPage, QWidget#practicePage, QWidget#column { background: @window@; }
        QLabel { color: @text@; background: transparent; }
        QLabel#title { font-size: 28pt; font-weight: bold; }
        QLabel#hint { color: @muted@; font-size: 12pt; }
        QLabel#heading { font-size: 17pt; font-weight: bold; padding-top: 8px; }
        QLabel#question { font-size: 14pt; font-weight: bold; }
        QLabel#answer { color: @muted@; }
        QLabel#status { background: @soft@; color: @text@; padding: 14px 18px; border: 3px solid @border@; border-radius: 12px; font-weight: bold; font-size: 15pt; }
        QLabel#status[active="true"] { background: @tealSoft@; color: @tealText@; border-color: @teal@; }
        QLabel#notice { background: @notice@; color: @noticeText@; padding: 14px 18px; border: 2px solid @noticeBorder@; border-radius: 10px; }

        QPushButton { min-height: 52px; padding: 6px 22px; border: 3px solid @border@; border-radius: 12px; background: @surface@; color: @text@; font-weight: bold; }
        QPushButton:hover { background: @accentSoft@; border-color: @accent@; color: @accentText@; }
        QPushButton:pressed { background: @accentSoftHover@; }
        QPushButton[kind="primary"] { min-height: 72px; font-size: 17pt; background: @teal@; color: @onColor@; border-color: @tealDark@; }
        QPushButton[kind="primary"]:hover, QPushButton[kind="primary"]:pressed { background: @tealHover@; color: @onColor@; border-color: @tealDark@; }
        QPushButton[kind="secondary"] { background: @accentSoft@; color: @accentText@; border-color: @accent@; }
        QPushButton[kind="secondary"]:hover { background: @accentSoftHover@; }
        QPushButton[kind="choice"] { min-height: 60px; font-size: 14pt; }
        QPushButton[kind="choice"]:checked { background: @accent@; color: @onColor@; border-color: @accentDark@; }
        QPushButton[kind="choice"]:checked:hover, QPushButton[kind="choice"]:checked:pressed { background: @accentHover@; color: @onColor@; }
        QPushButton[kind="link"] { background: transparent; border: 3px solid transparent; color: @accentText@; text-decoration: underline; text-align: left; padding-left: 12px; }
        QPushButton[kind="link"]:hover { background: @accentSoft@; border-color: transparent; }
        QPushButton[kind="quiet"] { background: transparent; color: @muted@; border: 3px solid @border@; }
        QPushButton[kind="quiet"]:hover, QPushButton[kind="quiet"]:pressed { background: @soft@; color: @text@; border-color: @border@; }
        QPushButton#updateBanner { min-height: 64px; background: @notice@; color: @noticeText@; border-color: @noticeBorder@; }
        QPushButton:disabled, QComboBox:disabled { background: @disabled@; color: @disabledText@; border-color: @cardBorder@; }

        QComboBox { min-height: 52px; padding: 4px 18px; border: 3px solid @border@; border-radius: 12px; background: @surface@; color: @text@; }
        QComboBox:hover { border-color: @accent@; }
        QComboBox::drop-down { width: 44px; border: none; border-left: 2px solid @cardBorder@; }
        QComboBox::down-arrow { image: url(@chevron@); width: 16px; height: 10px; }
        QComboBox QAbstractItemView { background: @surface@; color: @text@; selection-background-color: @accent@; selection-color: @onColor@; border: 2px solid @border@; padding: 4px; }
        QComboBox QAbstractItemView::item { min-height: 44px; padding: 0 12px; }

        QPushButton:focus, QComboBox:focus, QCheckBox:focus, QRadioButton:focus { border: 4px solid @focus@; }

        QGroupBox { background: @surface@; border: 2px solid @cardBorder@; border-radius: 16px; margin-top: 20px; padding: 30px 20px 20px; font-size: 15pt; font-weight: bold; }
        QGroupBox::title { subcontrol-origin: margin; left: 18px; top: 4px; padding: 0 10px; background: @surface@; color: @accentText@; border-radius: 6px; }
        QFrame#faq { background: transparent; border: none; border-left: 5px solid @cardBorder@; border-radius: 0; }

        QCheckBox, QRadioButton { min-height: 64px; padding: 8px 18px; spacing: 18px; color: @text@; border: 2px solid @border@; border-radius: 12px; background: @surface@; }
        QCheckBox:hover, QRadioButton:hover { background: @accentSoft@; border-color: @accent@; }
        QCheckBox:checked, QRadioButton:checked { background: @tealSoft@; border: 3px solid @teal@; color: @tealText@; font-weight: bold; }
        QRadioButton::indicator { width: 32px; height: 32px; border: 2px solid @border@; border-radius: 18px; background: @surface@; }
        QRadioButton::indicator:checked { background: @indicator@; border-color: @teal@; image: url(:/icons/check.xpm); }
        QCheckBox::indicator { width: 32px; height: 32px; border: 2px solid @border@; border-radius: 6px; background: @surface@; }
        QCheckBox::indicator:checked { background: @indicator@; border-color: @teal@; image: url(:/icons/check.xpm); }

        QSlider { min-height: 52px; background: transparent; }
        QSlider::groove:horizontal { height: 14px; background: @soft@; border: 2px solid @cardBorder@; border-radius: 8px; }
        QSlider::sub-page:horizontal { background: @accent@; border: 2px solid @accent@; border-radius: 8px; }
        QSlider::handle:horizontal { width: 36px; margin: -13px 0; background: @accent@; border: 4px solid @surface@; border-radius: 20px; }
        QSlider::handle:horizontal:hover { background: @accentHover@; }
        QSlider::handle:horizontal:focus { border: 4px solid @focus@; }

        QTabWidget::pane { border: none; border-top: 3px solid @cardBorder@; }
        QTabBar::tab { min-height: 56px; min-width: 120px; padding: 4px 28px; color: @muted@; font-size: 14pt; font-weight: bold; background: transparent; border: 2px solid transparent; border-bottom: 5px solid transparent; border-top-left-radius: 12px; border-top-right-radius: 12px; margin-right: 8px; }
        QTabBar::tab:hover { background: @soft@; color: @text@; }
        QTabBar::tab:selected { color: @accentText@; background: @accentSoft@; border-bottom: 5px solid @accent@; }
        QTabBar::tab:focus { border: 3px solid @focus@; }

        QScrollBar:vertical { background: @soft@; width: 22px; margin: 0; border-radius: 11px; }
        QScrollBar::handle:vertical { background: @border@; min-height: 56px; border: 5px solid @soft@; border-radius: 11px; }
        QScrollBar::handle:vertical:hover { background: @accent@; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QProgressBar { min-height: 32px; border: 2px solid @cardBorder@; border-radius: 8px; background: @soft@; color: @text@; text-align: center; font-weight: bold; }
        QProgressBar::chunk { background: @teal@; border-radius: 6px; }
        QMenu::item { min-height: 40px; padding: 4px 24px; }
        QMenu::item:selected { background: @accent@; color: @onColor@; }
        QLabel:disabled, QCheckBox:disabled, QRadioButton:disabled { color: @disabledText@; }
    )";
    const auto c = colors(dark);
    for (auto it = c.begin(); it != c.end(); ++it) sheet.replace("@" + it.key() + "@", it.value());
    return sheet;
}
