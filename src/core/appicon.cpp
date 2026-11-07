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

    // 底板：最初的蓝色渐变
    QLinearGradient gradient(body.topLeft(), body.bottomRight());
    gradient.setColorAt(0.0, QColor(0x60, 0xA5, 0xFA));
    gradient.setColorAt(0.5, QColor(0x3B, 0x82, 0xF6));
    gradient.setColorAt(1.0, QColor(0x1D, 0x4E, 0xD8));

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

    // 主图形：标准 SD 卡正面（圆角矩形 + 右上切角 + 写保护缺口 + 标签区 + 触点）
    const qreal cardW = s * 0.42;
    const qreal cardH = s * 0.54;
    const QRectF card(body.center().x() - cardW * 0.5,
                      body.center().y() - cardH * 0.5,
                      cardW, cardH);
    const qreal r = cardW * 0.13;
    const qreal chamfer = cardW * 0.30;   // 右上角的斜切

    QPainterPath cardShape;
    cardShape.moveTo(card.left() + r, card.top());
    cardShape.lineTo(card.right() - chamfer, card.top());
    cardShape.lineTo(card.right(), card.top() + chamfer);
    cardShape.lineTo(card.right(), card.bottom() - r);
    cardShape.quadTo(card.right(), card.bottom(), card.right() - r, card.bottom());
    cardShape.lineTo(card.left() + r, card.bottom());
    cardShape.quadTo(card.left(), card.bottom(), card.left(), card.bottom() - r);
    cardShape.lineTo(card.left(), card.top() + r);
    cardShape.quadTo(card.left(), card.top(), card.left() + r, card.top());
    cardShape.closeSubpath();

    painter.setBrush(QColor(255, 255, 255));
    painter.setPen(Qt::NoPen);
    painter.drawPath(cardShape);

    // 左侧写保护凹槽（从卡片边缘挖进去）
    {
        const qreal notchW = cardW * 0.13;
        const qreal notchH = cardH * 0.11;
        QPainterPath notch;
        notch.addRoundedRect(QRectF(card.left() - notchW * 0.55,
                                    card.top() + cardH * 0.30,
                                    notchW, notchH),
                             notchH * 0.22, notchH * 0.22);
        painter.setBrush(QColor(0x3B, 0x82, 0xF6));
        painter.drawPath(notch);
    }

    // 标签区（中间那块贴纸）
    const QRectF label(card.left() + cardW * 0.16,
                       card.top() + cardH * 0.16,
                       cardW * 0.68, cardH * 0.40);
    painter.setBrush(QColor(0x3B, 0x82, 0xF6));
    painter.drawRoundedRect(label, cardW * 0.07, cardW * 0.07);

    // 标签上的一条印刷线（白色，很淡）
    painter.setBrush(QColor(255, 255, 255, 170));
    const qreal lineX = label.left() + label.width() * 0.12;
    const qreal lineW = label.width() * 0.52;
    painter.drawRoundedRect(QRectF(lineX, label.center().y() - cardH * 0.022,
                                   lineW, cardH * 0.044),
                            cardH * 0.022, cardH * 0.022);

    // 底部触点：一排小竖条
    painter.setBrush(QColor(0x3B, 0x82, 0xF6));
    const int contacts = 6;
    const qreal gap = cardW * 0.055;
    const qreal barW = (cardW * 0.68 - gap * (contacts - 1)) / contacts;
    const qreal barTop = card.bottom() - cardH * 0.26;
    const qreal barH = cardH * 0.13;
    for (int i = 0; i < contacts; ++i) {
        const qreal x = card.left() + cardW * 0.16 + i * (barW + gap);
        painter.drawRoundedRect(QRectF(x, barTop, barW, barH), barW * 0.3, barW * 0.3);
    }
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
