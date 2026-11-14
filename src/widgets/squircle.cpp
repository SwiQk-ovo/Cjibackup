#include "widgets/squircle.h"

#include <QLinearGradient>
#include <QImage>
#include <QLayout>
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
    setAttribute(Qt::WA_NoSystemBackground, true);   // 不要 Qt 再刷一遍底色，否则卡片底部露一条深色边
    setAutoFillBackground(false);
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


/* ------------------------------------------------------------------ */
/*  毛玻璃工具：快速方框模糊（缩小→模糊→放大，性能足够）            */
/* ------------------------------------------------------------------ */
namespace {

QImage boxBlurSmall(const QImage &src, int radius)
{
    if (src.isNull() || radius <= 0)
        return src;

    const QImage in = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage out(in.size(), QImage::Format_ARGB32_Premultiplied);
    const int w = in.width();
    const int h = in.height();

    auto clampIdx = [](int v, int maxV) { return v < 0 ? 0 : (v >= maxV ? maxV - 1 : v); };

    // 水平
    QImage tmp(in.size(), QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(in.constScanLine(y));
        QRgb *dst = reinterpret_cast<QRgb *>(tmp.scanLine(y));
        int a = 0, r = 0, g = 0, b = 0;
        const int win = radius * 2 + 1;
        for (int x = -radius; x <= radius; ++x) {
            const QRgb c = line[clampIdx(x, w)];
            a += qAlpha(c); r += qRed(c); g += qGreen(c); b += qBlue(c);
        }
        for (int x = 0; x < w; ++x) {
            dst[x] = qRgba(r / win, g / win, b / win, a / win);
            const QRgb add = line[clampIdx(x + radius + 1, w)];
            const QRgb sub = line[clampIdx(x - radius, w)];
            a += qAlpha(add) - qAlpha(sub);
            r += qRed(add) - qRed(sub);
            g += qGreen(add) - qGreen(sub);
            b += qBlue(add) - qBlue(sub);
        }
    }

    // 垂直
    for (int x = 0; x < w; ++x) {
        int a = 0, r = 0, g = 0, b = 0;
        const int win = radius * 2 + 1;
        for (int y = -radius; y <= radius; ++y) {
            const QRgb c = reinterpret_cast<const QRgb *>(tmp.constScanLine(clampIdx(y, h)))[x];
            a += qAlpha(c); r += qRed(c); g += qGreen(c); b += qBlue(c);
        }
        for (int y = 0; y < h; ++y) {
            reinterpret_cast<QRgb *>(out.scanLine(y))[x] = qRgba(r / win, g / win, b / win, a / win);
            const QRgb add = reinterpret_cast<const QRgb *>(tmp.constScanLine(clampIdx(y + radius + 1, h)))[x];
            const QRgb sub = reinterpret_cast<const QRgb *>(tmp.constScanLine(clampIdx(y - radius, h)))[x];
            a += qAlpha(add) - qAlpha(sub);
            r += qRed(add) - qRed(sub);
            g += qGreen(add) - qGreen(sub);
            b += qBlue(add) - qBlue(sub);
        }
    }
    return out;
}

} // namespace
SquircleGroupBox::SquircleGroupBox(const QString &title, QWidget *parent)
    : QGroupBox(title, parent)
{
    // 整块卡片都是自绘的：把 QGroupBox 自带的背景/边框彻底关掉，
    // 否则会套出第二层框，还多出一段空白
    setProperty("squircle", true);
    setStyleSheet(QStringLiteral(
        "QGroupBox { background: transparent; border: none; margin: 0; padding: 0; }"));
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_NoSystemBackground, true);   // 不要 Qt 再刷一遍底色，否则卡片底部露一条深色边
    setAutoFillBackground(false);
    if (auto *l = layout()) {
        // 还没 layout，稍后再补
    }
}

void SquircleGroupBox::setRadius(qreal radius)
{
    m_radius = radius;
    update();
}

void SquircleGroupBox::ensureBackdrop()
{
    QWidget *top = window();
    if (!top || width() <= 0 || height() <= 0)
        return;
    if (m_backdropSize == size() && !m_backdrop.isNull())
        return;

    const QPoint posInTop = mapTo(top, QPoint(0, 0));
    QImage raw(size(), QImage::Format_ARGB32_Premultiplied);
    raw.fill(QColor(0x0E, 0x0F, 0x11));
    {
        QPainter p(&raw);
        // 只画顶层窗口自身（含背景光斑），不带子控件，避免把卡片自己拍进去
        top->render(&p, -posInTop, QRegion(QRect(posInTop, size())),
                    QWidget::DrawWindowBackground);
    }

    // 缩小 6 倍再模糊：又快又柔
    const int small = 6;
    QImage tiny = raw.scaled(qMax(1, raw.width() / small), qMax(1, raw.height() / small),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    tiny = boxBlurSmall(tiny, 3);
    tiny = boxBlurSmall(tiny, 3);
    m_backdrop = tiny.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_backdropSize = size();
}

void SquircleGroupBox::resizeEvent(QResizeEvent *event)
{
    QGroupBox::resizeEvent(event);
    m_backdrop = QImage();      // 尺寸变了，重新抓
    m_backdropSize = QSize();
}
void SquircleGroupBox::showEvent(QShowEvent *event)
{
    QGroupBox::showEvent(event);
    // 标题是自绘的，内容要避开标题那一条
    if (auto *l = layout())
        l->setContentsMargins(18, 44, 18, 12);
}

void SquircleGroupBox::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const QPainterPath shape = Squircle::path(r, m_radius);

    // 平面卡片：一个纯色面 + 一道极淡描边，没有模糊、没有渐变、没有投影
    painter.fillPath(shape, QColor(0x17, 0x18, 0x1A));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 16), 1.0));
    painter.drawPath(shape);

    const QString text = title();
    if (!text.isEmpty()) {
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(13);
        painter.setFont(f);
        painter.setPen(QColor(0xC7, 0xCF, 0xDB));
        painter.drawText(QRect(18, 13, width() - 36, 24),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
    }
}

