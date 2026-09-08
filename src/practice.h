// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QWidget>
#include <QVector>
class Practice final : public QWidget {
    Q_OBJECT
public:
    explicit Practice(QWidget *parent = nullptr);
    void clear();
protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
private:
    QPointF target() const;
    QVector<QPointF> trail;
    int targetIndex = 0;
};
