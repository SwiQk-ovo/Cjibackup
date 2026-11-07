#include "splash3d.h"

#include <QImage>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTransform>
#include <QtMath>

namespace Splash3d {

QPolygonF projectQuad(const QRectF &rect, qreal rotX, qreal rotY, qreal scale, qreal fov)
{
    const qreal rx = qDegreesToRadians(rotX);
    const qreal ry = qDegreesToRadians(rotY);
    const qreal cosX = qCos(rx);
    const qreal sinX = qSin(rx);
    const qreal cosY = qCos(ry);
    const qreal sinY = qSin(ry);

    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal hx = rect.width() * 0.5 * scale;
    const qreal hy = rect.height() * 0.5 * scale;

    const QPointF corners[4] = {QPointF(-hx, -hy), QPointF(hx, -hy),
                                QPointF(hx, hy), QPointF(-hx, hy)};

    QPolygonF quad;
    quad.reserve(4);
    for (const QPointF &corner : corners) {
        const qreal x = corner.x();
        const qreal y = corner.y();

        // 绕 X 轴
        const qreal y1 = y * cosX;
        const qreal z1 = y * sinX;

        // 绕 Y 轴
        const qreal x2 = x * cosY + z1 * sinY;
        const qreal z2 = -x * sinY + z1 * cosY;

        const qreal k = fov / qMax<qreal>(1.0, fov + z2);
        quad << QPointF(cx + x2 * k, cy + y1 * k);
    }
    return quad;
}

bool drawPixmap3D(QPainter &painter, const QPixmap &pixmap, const QRectF &rect,
                  qreal rotX, qreal rotY, qreal scale, qreal opacity, qreal fov)
{
    if (pixmap.isNull())
        return false;

    const QPolygonF quad = projectQuad(rect, rotX, rotY, scale, fov);
    const QRectF source(0, 0, pixmap.width(), pixmap.height());

    QTransform transform;
    if (!QTransform::quadToQuad(QPolygonF{source.topLeft(), source.topRight(),
                                          source.bottomRight(), source.bottomLeft()},
                                quad, transform))
        return false;

    painter.save();
    painter.setOpacity(opacity);
    painter.setTransform(transform, true);
    painter.drawPixmap(0, 0, pixmap);
    painter.restore();
    return true;
}

bool drawPixmapSlice3D(QPainter &painter, const QPixmap &pixmap, const QRectF &source,
                       const QRectF &target, qreal rotX, qreal rotY, qreal scale,
                       qreal opacity, qreal fov)
{
    if (pixmap.isNull() || source.isEmpty())
        return false;

    const QPolygonF quad = projectQuad(target, rotX, rotY, scale, fov);
    QTransform transform;
    if (!QTransform::quadToQuad(QPolygonF{source.topLeft(), source.topRight(),
                                          source.bottomRight(), source.bottomLeft()},
                                quad, transform))
        return false;

    painter.save();
    painter.setOpacity(opacity);
    painter.setTransform(transform, true);
    // 变换是把 source 矩形本身映射到 quad，所以这里要画在 source 的位置
    painter.drawPixmap(source, pixmap, source);
    painter.restore();
    return true;
}

void drawReflection(QPainter &painter, const QPixmap &pixmap, const QRectF &rect,
                    qreal opacity, const QColor &fadeColor, qreal gap, qreal heightFactor)
{
    if (pixmap.isNull() || opacity <= 0.001)
        return;

    const QRectF full(rect.left(), rect.bottom() + gap, rect.width(), rect.height());
    // 只取倒影最上面的一小段，避免糊到标题上
    const QRectF mirrored(full.left(), full.top(), full.width(),
                          full.height() * qBound<qreal>(0.1, heightFactor, 1.0));
    const QPolygonF quad = projectQuad(mirrored, 0, 0, 1.0);
    const qreal cropped = qBound<qreal>(0.1, heightFactor, 1.0);
    const QRectF source(0, 0, pixmap.width(), pixmap.height() * cropped);

    QTransform transform;
    if (!QTransform::quadToQuad(QPolygonF{source.topLeft(), source.topRight(),
                                          source.bottomRight(), source.bottomLeft()},
                                quad, transform))
        return;

    painter.save();
    painter.setOpacity(opacity);
    painter.setTransform(transform, true);
    painter.drawImage(QRectF(0, 0, source.width(), source.height()), pixmap.toImage().mirrored(false, true),
                      source);
    painter.restore();

    // 向下渐隐，避免倒影糊到标题上
    painter.save();
    QPainterPath clip;
    clip.addPolygon(quad);
    clip.closeSubpath();
    painter.setClipPath(clip);

    QLinearGradient fade(mirrored.topLeft(), mirrored.bottomLeft());
    QColor start = fadeColor;
    start.setAlpha(20);
    QColor end = fadeColor;
    end.setAlpha(255);
    fade.setColorAt(0.0, start);
    fade.setColorAt(0.35, QColor(fadeColor.red(), fadeColor.green(), fadeColor.blue(), 120));
    fade.setColorAt(0.7, QColor(fadeColor.red(), fadeColor.green(), fadeColor.blue(), 220));
    fade.setColorAt(1.0, end);
    painter.fillRect(mirrored, fade);
    painter.restore();
}

void drawContactShadow(QPainter &painter, const QPointF &center, qreal radiusX, qreal radiusY,
                       qreal alpha)
{
    if (alpha <= 0.001)
        return;

    QRadialGradient gradient(center, qMax(radiusX, radiusY));
    gradient.setColorAt(0.0, QColor(0, 0, 0, int(210 * alpha)));
    gradient.setColorAt(0.55, QColor(0, 0, 0, int(90 * alpha)));
    gradient.setColorAt(1.0, QColor(0, 0, 0, 0));

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(center, radiusX, radiusY);
    painter.restore();
}

void drawSpecular(QPainter &painter, const QPolygonF &quad, qreal phase, const QColor &tint)
{
    if (quad.isEmpty())
        return;

    const QRectF bounds = quad.boundingRect();
    QPainterPath clip;
    clip.addPolygon(quad);
    clip.closeSubpath();

    const qreal band = bounds.width() * 0.75;
    const qreal travel = bounds.width() + band * 2.0;
    const qreal x = bounds.left() - band + travel * qBound<qreal>(0.0, phase, 1.0);

    QLinearGradient gradient(x, bounds.top(), x + band, bounds.bottom());
    gradient.setColorAt(0.0, QColor(255, 255, 255, 0));
    gradient.setColorAt(0.42, QColor(tint.red(), tint.green(), tint.blue(), 60));
    gradient.setColorAt(0.5, QColor(255, 255, 255, 130));
    gradient.setColorAt(0.58, QColor(tint.red(), tint.green(), tint.blue(), 60));
    gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

    painter.save();
    painter.setClipPath(clip);
    painter.fillRect(bounds.adjusted(-2, -2, 2, 2), gradient);
    painter.restore();
}

} // namespace Splash3d
