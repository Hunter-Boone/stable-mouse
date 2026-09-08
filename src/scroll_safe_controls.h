// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QComboBox>
#include <QSlider>
#include <QWheelEvent>
#include <QAbstractScrollArea>
#include <QApplication>

inline void scrollContainingPage(QWidget *control, QWheelEvent *event) {
    for (auto *parent = control->parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto *area = qobject_cast<QAbstractScrollArea *>(parent)) {
            QWheelEvent forwarded(area->viewport()->mapFromGlobal(event->globalPosition().toPoint()),
                                  event->globalPosition(), event->pixelDelta(), event->angleDelta(),
                                  event->buttons(), event->modifiers(), event->phase(), event->inverted(), event->source());
            QApplication::sendEvent(area->viewport(), &forwarded);
            event->accept();
            return;
        }
    }
    event->ignore();
}

// Let wheel and touchpad scrolling reach the containing page, even when the
// control has focus. Click/drag and keyboard adjustments still work normally.
class ScrollSafeSlider final : public QSlider {
public:
    explicit ScrollSafeSlider(Qt::Orientation orientation) : QSlider(orientation) { setFocusPolicy(Qt::StrongFocus); }
protected:
    void wheelEvent(QWheelEvent *event) override { scrollContainingPage(this, event); }
};
class ScrollSafeComboBox final : public QComboBox {
public:
    ScrollSafeComboBox() { setFocusPolicy(Qt::StrongFocus); }
protected:
    void wheelEvent(QWheelEvent *event) override { scrollContainingPage(this, event); }
};
