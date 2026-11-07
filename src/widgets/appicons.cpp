#include "widgets/appicons.h"

#include <QPainter>
#include <QPainterPath>

namespace AppIcons {

namespace {

// 2 倍绘制再缩回来，小尺寸也锐利
QPixmap render(Glyph glyph, int size, const QColor &color)
{
    const int scale = 2;
    const int side = size * scale;

    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = side;
    QPen pen(color, qMax<qreal>(1.4, s * 0.075));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (glyph) {
    case Glyph::Camera: {
        const QRectF body(s * 0.12, s * 0.32, s * 0.76, s * 0.46);
        painter.drawRoundedRect(body, s * 0.10, s * 0.10);
        // 取景凸起
        QPainterPath bump;
        bump.addRoundedRect(QRectF(s * 0.36, s * 0.22, s * 0.28, s * 0.14),
                            s * 0.05, s * 0.05);
        painter.drawPath(bump);
        // 镜头
        painter.drawEllipse(QPointF(s * 0.5, s * 0.55), s * 0.15, s * 0.15);
        break;
    }
    case Glyph::Tag: {
        // 标签：一个转 45° 的圆角方块 + 中间的孔
        painter.save();
        painter.translate(s * 0.5, s * 0.5);
        painter.rotate(45);
        const qreal half = s * 0.28;
        painter.drawRoundedRect(QRectF(-half, -half, half * 2, half * 2),
                                s * 0.08, s * 0.08);
        painter.restore();
        painter.setBrush(color);
        painter.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.055, s * 0.055);
        break;
    }
    case Glyph::Card: {
        // SD 卡：右上切角的小卡片
        QPainterPath card;
        const qreal left = s * 0.28, right = s * 0.72;
        const qreal top = s * 0.14, bottom = s * 0.86;
        const qreal r = s * 0.07, chamfer = s * 0.16;
        card.moveTo(left + r, top);
        card.lineTo(right - chamfer, top);
        card.lineTo(right, top + chamfer);
        card.lineTo(right, bottom - r);
        card.quadTo(right, bottom, right - r, bottom);
        card.lineTo(left + r, bottom);
        card.quadTo(left, bottom, left, bottom - r);
        card.lineTo(left, top + r);
        card.quadTo(left, top, left + r, top);
        card.closeSubpath();
        painter.drawPath(card);
        // 触点
        painter.setBrush(color);
        const qreal barW = s * 0.055;
        const qreal gap = s * 0.04;
        for (int i = 0; i < 3; ++i) {
            const qreal x = left + s * 0.07 + i * (barW + gap);
            painter.drawRoundedRect(QRectF(x, bottom - s * 0.20, barW, s * 0.10),
                                    barW * 0.4, barW * 0.4);
        }
        break;
    }
    }

    painter.end();

    return pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace

QPixmap pixmap(Glyph glyph, int size, const QColor &color)
{
    return render(glyph, size, color);
}

QIcon icon(Glyph glyph, int size, const QColor &color)
{
    QIcon result;
    result.addPixmap(render(glyph, size, color));
    result.addPixmap(render(glyph, size * 2, color), QIcon::Normal, QIcon::Off);
    return result;
}

} // namespace AppIcons
