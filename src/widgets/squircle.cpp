#include "widgets/squircle.h"

#include <QLinearGradient>
#include <QPainter>
#include <QStyleOptionButton>
#include <QtMath>

#include <cmath>

namespace {

constexpr int kStepsPerCorner = 18;

QPointF pointOnCorner(const QPointF &center, qreal radius, qreal degrees, qreal exponent)
{
    const qreal rad = qDegreesToRadians(degrees);
    const qreal ct = std::cos(rad);
    const qreal st = std::sin(rad);

    // |x/a|^n + |y/a|^n = 1 的参数形式：x = a*sign(cos)|cos|^(2/n)
    const qreal power = 2.0 / qMax<qreal>(2.0, exponent);
    const qreal x = std::copysign(std::pow(std::abs(ct), power), ct);
    const qreal y = std::copysign(std::pow(std::abs(st), power), st);

    return QPointF(center.x() + radius * x, center.y() - radius * y);
}

} // namespace

namespace Squircle {

QPainterPath path(const QRectF &rect, qreal radius, qreal exponent)
{
    QPainterPath path;
    if (rect.isEmpty())
        return path;

    const qreal r = qMin(radius, qMin(rect.width(), rect.height()) / 2.0);
    if (r <= 0.6) {
        path.addRect(rect);
        return path;
    }

    const qreal left = rect.left() + r;
    const qreal right = rect.right() - r;
    const qreal top = rect.top() + r;
    const qreal bottom = rect.bottom() - r;

    // 从左上角的切点开始，顺时针把四个角串起来
    path.moveTo(left, rect.top());

    const QPointF corners[4] = {
        QPointF(left, top),     // 左上：90° → 180°
        QPointF(left, bottom),  // 左下：180° → 270°
        QPointF(right, bottom), // 右下：270° → 360°
        QPointF(right, top)     // 右上：360° → 450°
    };

    for (int corner = 0; corner < 4; ++corner) {
        const qreal from = 90.0 + corner * 90.0;
        for (int step = 1; step <= kStepsPerCorner; ++step) {
            const qreal t = from + 90.0 * qreal(step) / qreal(kStepsPerCorner);
            path.lineTo(pointOnCorner(corners[corner], r, t, exponent));
        }
    }

    path.closeSubpath();
    return path;
}

} // namespace Squircle

/* ------------------------------------------------------------------ */

SquircleCard::SquircleCard(QWidget *parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
}

void SquircleCard::setRadius(qreal radius)
{
    m_radius = radius;
    update();
}

void SquircleCard::setFill(const QColor &color)
{
    m_fill = color;
    m_fillBottom = QColor();
    update();
}

void SquircleCard::setFillGradient(const QColor &top, const QColor &bottom)
{
    m_fill = top;
    m_fillBottom = bottom;
    update();
}

void SquircleCard::setBorder(const QColor &color, qreal width)
{
    m_border = color;
    m_borderWidth = width;
    update();
}

void SquircleCard::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath shape = Squircle::path(body, m_radius);

    if (m_fill.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        if (m_fillBottom.isValid()) {
            QLinearGradient gradient(body.topLeft(), body.bottomLeft());
            gradient.setColorAt(0.0, m_fill);
            gradient.setColorAt(1.0, m_fillBottom);
            painter.setBrush(gradient);
        } else {
            painter.setBrush(m_fill);
        }
        painter.drawPath(shape);
    }
    if (m_borderWidth > 0 && m_border.alpha() > 0) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_border, m_borderWidth));
        painter.drawPath(shape);
    }
}

/* ------------------------------------------------------------------ */

SquircleGroupBox::SquircleGroupBox(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
{
    // 让 QSS 不要再画自己的背景/边框，只留标题
    setProperty("squircle", true);
}

void SquircleGroupBox::setRadius(qreal radius)
{
    m_radius = radius;
    update();
}

void SquircleGroupBox::paintEvent(QPaintEvent *event)
{
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QPainterPath shape = Squircle::path(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                                                  m_radius);
        painter.fillPath(shape, QColor(0x1C, 0x21, 0x2C));
        painter.setPen(QPen(QColor(0x2A, 0x2F, 0x3A), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(shape);
    }

    // QSS 已经把这俩的背景/边框关掉，这里只会画标题
    QGroupBox::paintEvent(event);
}

