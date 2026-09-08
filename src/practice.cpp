// SPDX-License-Identifier: GPL-3.0-only
#include "practice.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
Practice::Practice(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true); setMinimumSize(280, 240);
    setAccessibleName("Mouse practice area");
    setAccessibleDescription("Move the pointer and click the large circular target. Its position changes after a click. No results are saved.");
}
QPointF Practice::target() const {
    const QPointF positions[] = {{.5, .5}, {.25, .3}, {.75, .65}, {.25, .7}, {.75, .3}};
    const auto p = positions[targetIndex % 5];
    return {p.x() * width(), p.y() * height()};
}
void Practice::clear() { trail.clear(); targetIndex = 0; update(); }
void Practice::mouseMoveEvent(QMouseEvent *event) {
    trail.append(event->position());
    if (trail.size() > 180) trail.removeFirst();
    update();
}
void Practice::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && QLineF(event->position(), target()).length() <= 34) {
        ++targetIndex; trail.clear(); update();
    }
}
void Practice::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());
    p.setPen(QPen(palette().mid().color(), 1));
    for (int x = 20; x < width(); x += 24)
        for (int y = 20; y < height(); y += 24) p.drawPoint(x, y);
    if (!trail.empty()) {
        QPainterPath path; path.moveTo(trail.first());
        for (const auto &point : trail) path.lineTo(point);
        p.setPen(QPen(palette().highlight().color(), 2)); p.drawPath(path);
    }
    p.setBrush(palette().highlight()); p.setPen(Qt::NoPen); p.drawEllipse(target(), 34, 34);
    p.setPen(QPen(palette().highlightedText().color(), 2));
    p.drawLine(target() - QPointF(9, 0), target() + QPointF(9, 0));
    p.drawLine(target() - QPointF(0, 9), target() + QPointF(0, 9));
}
