#include "appicon.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

namespace AppIcon {

QPixmap logoPixmap(int size)
{
    if (size <= 0)
        size = 256;

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal s = size;
    const qreal inset = s * 0.035;
    const QRectF body(inset, inset, s - inset * 2, s - inset * 2);
    const qreal radius = body.width() * 0.24;

    // 底板：蓝色渐变圆角方块
    QLinearGradient gradient(body.topLeft(), body.bottomRight());
    gradient.setColorAt(0.0, QColor(0x60, 0xA5, 0xFA));
    gradient.setColorAt(0.5, QColor(0x3B, 0x82, 0xF6));
    gradient.setColorAt(1.0, QColor(0x1D, 0x4E, 0xD8));

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(body, radius, radius);

    // 顶部一层柔光，让方块不那么平
    QRadialGradient gloss(body.center() + QPointF(0, -body.height() * 0.22),
                          body.width() * 0.72);
    gloss.setColorAt(0.0, QColor(255, 255, 255, 60));
    gloss.setColorAt(0.6, QColor(255, 255, 255, 12));
    gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(gloss);
    painter.drawRoundedRect(body, radius, radius);

    // 外圈细高光，边缘更利落
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 40), qMax<qreal>(1.0, s * 0.012)));
    painter.drawRoundedRect(body.adjusted(s * 0.012, s * 0.012, -s * 0.012, -s * 0.012),
                            radius * 0.92, radius * 0.92);

    // 主体字：「拷」
    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(int(s * 0.52));
    font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    painter.setFont(font);

    const QRectF textRect = body.adjusted(0, s * 0.02, 0, s * 0.04);

    // 先画一层淡淡的下投影，小尺寸时也不糊
    painter.setPen(QColor(10, 30, 70, 90));
    painter.drawText(textRect.translated(0, s * 0.012), Qt::AlignCenter, QStringLiteral("拷"));
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(textRect, Qt::AlignCenter, QStringLiteral("拷"));


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
