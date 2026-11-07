#include "appicon.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QtMath>

namespace AppIcon {

QPixmap logoPixmap(int size)
{
    if (size <= 0)
        size = 256;

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal s = size;
    const qreal inset = s * 0.045;
    const QRectF body(inset, inset, s - inset * 2, s - inset * 2);
    const qreal radius = body.width() * 0.235;

    // 底板：琥珀色渐变（Kopal = 柯巴脂，把照片封存起来）
    QLinearGradient gradient(body.topLeft(), body.bottomRight());
    gradient.setColorAt(0.0, QColor(0xFF, 0xC9, 0x7A));
    gradient.setColorAt(0.45, QColor(0xF5, 0x8A, 0x1A));
    gradient.setColorAt(1.0, QColor(0xC2, 0x41, 0x0C));

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(body, radius, radius);

    // 顶部柔光
    QRadialGradient gloss(body.center() + QPointF(0, -body.height() * 0.28),
                          body.width() * 0.78);
    gloss.setColorAt(0.0, QColor(255, 255, 255, 70));
    gloss.setColorAt(0.55, QColor(255, 255, 255, 16));
    gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(gloss);
    painter.drawRoundedRect(body, radius, radius);

    // 主图形：一台极简相机（机身 + 顶部取景凸起 + 镜头）
    const qreal bodyW = s * 0.54;
    const qreal bodyH = s * 0.38;
    const QRectF camera(body.center().x() - bodyW * 0.5,
                        body.center().y() - bodyH * 0.42,
                        bodyW, bodyH);
    const qreal camRadius = bodyH * 0.26;

    QPainterPath cameraShape;
    cameraShape.addRoundedRect(camera, camRadius, camRadius);

    // 顶部取景器凸起
    const qreal bumpW = bodyW * 0.30;
    const qreal bumpH = bodyH * 0.22;
    const QRectF bump(camera.center().x() - bumpW * 0.5,
                      camera.top() - bumpH + camRadius * 0.35, bumpW, bumpH * 1.4);
    QPainterPath bumpShape;
    bumpShape.addRoundedRect(bump, bumpH * 0.45, bumpH * 0.45);
    cameraShape = cameraShape.united(bumpShape);

    // 镜头：挖掉一个圆，再放一个小白点
    const QPointF lensCenter(camera.center().x(), camera.center().y() + bodyH * 0.04);
    const qreal lensR = bodyH * 0.34;
    QPainterPath lensHole;
    lensHole.addEllipse(lensCenter, lensR, lensR);
    cameraShape = cameraShape.subtracted(lensHole);

    painter.setBrush(QColor(255, 255, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPath(cameraShape);

    painter.setBrush(QColor(255, 255, 255));
    painter.drawEllipse(lensCenter, lensR * 0.42, lensR * 0.42);

    // 右上角的闪光小圆点
    painter.drawEllipse(QPointF(camera.right() - bodyW * 0.12,
                                camera.top() + bodyH * 0.20),
                        bodyH * 0.055, bodyH * 0.055);
    // 外圈细高光，边缘更利落
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 55), qMax<qreal>(1.0, s * 0.010)));
    painter.drawRoundedRect(body.adjusted(s * 0.010, s * 0.010, -s * 0.010, -s * 0.010),
                            radius * 0.94, radius * 0.94);

    painter.end();
    return pixmap;
}

QIcon applicationIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        icon.addPixmap(logoPixmap(size));
    return icon;
}

} // namespace AppIcon
