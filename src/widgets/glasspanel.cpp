#include "widgets/glasspanel.h"

#include <QImage>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>

namespace {

// 圆角矩形的有符号距离（内部为负）
qreal roundedRectDistance(qreal x, qreal y, const QRectF &r, qreal radius)
{
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal hx = r.width() * 0.5 - radius;
    const qreal hy = r.height() * 0.5 - radius;
    const qreal qx = qAbs(x - cx) - hx;
    const qreal qy = qAbs(y - cy) - hy;
    const qreal ax = qMax(qx, 0.0);
    const qreal ay = qMax(qy, 0.0);
    return qSqrt(ax * ax + ay * ay) + qMin(qMax(qx, qy), 0.0) - radius;
}

} // namespace

GlassPanel::GlassPanel(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_timer = new QTimer(this);
    m_timer->setInterval(40);
    connect(m_timer, &QTimer::timeout, this, &GlassPanel::tickSheen);
}

void GlassPanel::setRadius(qreal radius)
{
    m_radius = radius;
    m_cache = QImage();
    update();
}

void GlassPanel::setTint(const QColor &color)
{
    m_tint = color;
    m_cache = QImage();
    update();
}

void GlassPanel::setRefraction(int pixels)
{
    m_refraction = qBound(0, pixels, 40);
    m_cache = QImage();
    update();
}

void GlassPanel::setSheenEnabled(bool enabled)
{
    m_sheen = enabled;
    if (!enabled)
        m_timer->stop();
    else if (isVisible())
        m_timer->start();
    update();
}

void GlassPanel::setSheenStrength(qreal strength)
{
    m_sheenStrength = qBound<qreal>(0.0, strength, 0.4);
    update();
}

void GlassPanel::setContentMargins(int left, int top, int right, int bottom)
{
    if (auto *l = layout())
        l->setContentsMargins(left, top, right, bottom);
}

void GlassPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_sheen)
        m_timer->start();
}

void GlassPanel::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_timer->stop();
}

void GlassPanel::tickSheen()
{
    m_phase += 0.012;
    if (m_phase > 1.0)
        m_phase -= 1.0;
    update();
}

void GlassPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    m_cache = QImage();   // 尺寸变了，折射要重算
}

// 取背后的画面 → 按位移贴图做折射 → 上色/高光/描边
QImage GlassPanel::buildRefracted()
{
    const qreal dpr = devicePixelRatioF();
    const int w = qMax(1, int(width() * dpr));
    const int h = qMax(1, int(height() * dpr));

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);

    // 1) 抓背景：把父控件这一块画出来（不含子控件）
    QImage backdrop(w, h, QImage::Format_ARGB32_Premultiplied);
    backdrop.setDevicePixelRatio(dpr);
    backdrop.fill(QColor(0x0E, 0x0F, 0x11));
    if (QWidget *p = parentWidget()) {
        QPainter bp(&backdrop);
        bp.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p->render(&bp, -pos(), geometry(), QWidget::DrawWindowBackground);
        bp.end();
    }

    // 2) 位移贴图 + 采样：靠近边缘的地方把背景向里"挤"一点，形成折射
    const QRectF shape(0, 0, width(), height());
    const qreal radius = qMin(m_radius, qMin(shape.width(), shape.height()) * 0.5);
    const qreal band = qMax<qreal>(8.0, qMin(shape.width(), shape.height()) * 0.45);
    const qreal maxShift = m_refraction * dpr;

    QPainter op(&out);
    op.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 逐像素处理（面板都不大，缓冲区有缓存，只在尺寸变化时算一次）
    const QImage src = backdrop.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        const qreal ly = y / dpr;
        for (int x = 0; x < w; ++x) {
            const qreal lx = x / dpr;
            const qreal d = roundedRectDistance(lx, ly, shape, radius);
            if (d > 0) {
                line[x] = 0;   // 圆角外
                continue;
            }
            const qreal t = qBound<qreal>(0.0, -d / band, 1.0);   // 0=边缘 1=内部
            // 边缘处偏移最大，往内侧挤，像透镜把背景压弯
            const qreal shift = maxShift * (1.0 - t) * (1.0 - t);
            // 近似法线：从中心指向边缘的方向（简单够用）
            const qreal nx = (lx - shape.center().x()) / (shape.width() * 0.5);
            const qreal ny = (ly - shape.center().y()) / (shape.height() * 0.5);
            const int sx = qBound(0, int(x + nx * shift), w - 1);
            const int sy = qBound(0, int(y + ny * shift), h - 1);
            line[x] = src.pixel(sx, sy);
        }
    }

    // 3) 玻璃本体：折射后的背景 + 很淡的白
    op.setClipPath([&]() {
        QPainterPath path;
        path.addRoundedRect(shape.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        return path;
    }());
    op.drawImage(0, 0, result);

    QPainterPath clip;
    clip.addRoundedRect(shape.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    op.fillPath(clip, QColor(m_tint));

    // 4) 顶部高光
    QLinearGradient topGloss(shape.topLeft(), shape.bottomLeft());
    topGloss.setColorAt(0.0, QColor(255, 255, 255, 54));
    topGloss.setColorAt(0.35, QColor(255, 255, 255, 14));
    topGloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    op.fillPath(clip, topGloss);

    // 5) 液态流光
    if (m_sheen && m_sheenStrength > 0.001) {
        const qreal wpx = shape.width();
        const qreal bandW = wpx * 0.34;
        const qreal sx = -bandW + (wpx + bandW * 2) * m_phase;
        QLinearGradient sheen(sx, shape.top(), sx + bandW, shape.bottom());
        sheen.setColorAt(0.0, QColor(255, 255, 255, 0));
        sheen.setColorAt(0.5, QColor(255, 255, 255, int(255 * m_sheenStrength * 0.5)));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        op.fillPath(clip, sheen);
    }

    // 6) 描边：上亮下暗
    QLinearGradient stroke(shape.topLeft(), shape.bottomLeft());
    stroke.setColorAt(0.0, QColor(255, 255, 255, 92));
    stroke.setColorAt(0.5, QColor(255, 255, 255, 30));
    stroke.setColorAt(1.0, QColor(255, 255, 255, 14));
    op.setPen(QPen(QBrush(stroke), 1.0));
    op.setBrush(Qt::NoBrush);
    op.drawPath(clip);

    op.end();
    return out;
}


// —— 可复用的玻璃绘制：把 backdrop 折射后画进 rect ——
void GlassPanel::paintGlass(QPainter *p, const QRectF &rect, const QImage &backdrop,
                            qreal radius, int refractionPx, const QColor &tint,
                            qreal sheenPhase, qreal sheenStrength)
{
    if (rect.isEmpty())
        return;

    const qreal dpr = p->device() ? p->device()->devicePixelRatioF() : 1.0;
    const int w = qMax(1, int(rect.width()));
    const int h = qMax(1, int(rect.height()));

    QImage src = backdrop;
    if (src.isNull()) {
        src = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
        src.fill(QColor(0x0E, 0x0F, 0x11));
    } else if (src.size() != QSize(w, h)) {
        src = src.scaled(QSize(w, h), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    src = src.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QImage refracted(w, h, QImage::Format_ARGB32_Premultiplied);
    refracted.fill(Qt::transparent);

    QPainterPath clip;
    clip.addRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p->save();
    p->setClipPath(clip);

    const qreal band = qMax<qreal>(8.0, qMin(rect.width(), rect.height()) * 0.42);
    const QRectF local(0, 0, w, h);
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(refracted.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const qreal d = roundedRectDistance(x, y, local, qMin(radius, qMin(w, h) * 0.5));
            if (d > 0)
                continue;
            const qreal t = qBound<qreal>(0.0, -d / band, 1.0);
            const qreal shift = refractionPx * (1.0 - t) * (1.0 - t);
            const qreal nx = (x - local.center().x()) / (local.width() * 0.5);
            const qreal ny = (y - local.center().y()) / (local.height() * 0.5);
            const int sx = qBound(0, int(x + nx * shift), w - 1);
            const int sy = qBound(0, int(y + ny * shift), h - 1);
            line[x] = src.pixel(sx, sy);
        }
    }

    p->drawImage(rect.topLeft(), refracted);
    p->fillPath(clip, tint);

    QLinearGradient gloss(rect.topLeft(), rect.bottomLeft());
    gloss.setColorAt(0.0, QColor(255, 255, 255, 54));
    gloss.setColorAt(0.4, QColor(255, 255, 255, 14));
    gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
    p->fillPath(clip, gloss);

    if (sheenStrength > 0.001) {
        const qreal bandW = rect.width() * 0.34;
        const qreal sx = rect.left() - bandW + (rect.width() + bandW * 2) * sheenPhase;
        QLinearGradient sheen(sx, rect.top(), sx + bandW, rect.bottom());
        sheen.setColorAt(0.0, QColor(255, 255, 255, 0));
        sheen.setColorAt(0.5, QColor(255, 255, 255, int(255 * sheenStrength * 0.5)));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        p->fillPath(clip, sheen);
    }

    QLinearGradient stroke(rect.topLeft(), rect.bottomLeft());
    stroke.setColorAt(0.0, QColor(255, 255, 255, 92));
    stroke.setColorAt(0.5, QColor(255, 255, 255, 30));
    stroke.setColorAt(1.0, QColor(255, 255, 255, 14));
    p->setPen(QPen(QBrush(stroke), 1.0));
    p->setBrush(Qt::NoBrush);
    p->drawPath(clip);
    p->restore();
    Q_UNUSED(dpr);
}
void GlassPanel::paintEvent(QPaintEvent *)
{
    if (m_cache.isNull() || m_cache.size() != size() * devicePixelRatioF()) {
        // 背后是页面底图，一般不变；这里只算一次，之后复用
        m_cache = buildRefracted();
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 阴影：拉开与背景的距离
    const qreal radius = qMin(m_radius, qMin(width(), height()) * 0.5);
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()).adjusted(0.5, 2.5, -0.5, 0.5), radius, radius);
    p.fillPath(shape, QColor(0, 0, 0, 40));

    p.drawImage(0, 0, m_cache);

    // 流光每帧都变，单独画（不重算折射）
    if (m_sheen && m_sheenStrength > 0.001) {
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        const qreal bandW = width() * 0.34;
        const qreal sx = -bandW + (width() + bandW * 2) * m_phase;
        QLinearGradient sheen(sx, 0, sx + bandW, height());
        sheen.setColorAt(0.0, QColor(255, 255, 255, 0));
        sheen.setColorAt(0.5, QColor(255, 255, 255, int(255 * m_sheenStrength * 0.5)));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        p.fillPath(clip, sheen);
    }
}
