#include "splashwindow.h"

#include "core/appicon.h"
#include "core/settingsmanager.h"
#include "core/version.h"
#include "splash3d.h"
#include "widgets/squircle.h"

#include <QBrush>
#include <QConicalGradient>
#include <QGuiApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <QScreen>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QtMath>

#ifdef CJI_HAS_MULTIMEDIA
#  include <QMediaPlayer>
#  include <QVideoSink>
#  include <QVideoFrame>
#endif

#include <cmath>
#include <random>

namespace {

constexpr int kCardWidth = 460;
constexpr int kCardHeight = 280;
constexpr int kFadeOutMs = 320;
constexpr qreal kLogoSize = 92.0;

bool isVideoFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("mp4") || suffix == QStringLiteral("mov")
        || suffix == QStringLiteral("m4v") || suffix == QStringLiteral("webm")
        || suffix == QStringLiteral("mkv") || suffix == QStringLiteral("avi");
}

qreal clamp01(qreal v)
{
    return qBound<qreal>(0.0, v, 1.0);
}

qreal easeOutCubic(qreal t)
{
    t = clamp01(t);
    const qreal inv = 1.0 - t;
    return 1.0 - inv * inv * inv;
}

qreal easeInCubic(qreal t)
{
    t = clamp01(t);
    return t * t * t;
}

qreal easeInOutCubic(qreal t)
{
    t = clamp01(t);
    return t < 0.5 ? 4.0 * t * t * t : 1.0 - qPow(-2.0 * t + 2.0, 3.0) / 2.0;
}

qreal easeOutBack(qreal t)
{
    const qreal c1 = 1.70158;
    const qreal c3 = c1 + 1.0;
    t = clamp01(t);
    return 1.0 + c3 * qPow(t - 1.0, 3.0) + c1 * qPow(t - 1.0, 2.0);
}

// 把整体进度里的一段映射成 0..1
qreal seg(qreal p, qreal from, qreal to)
{
    if (to <= from)
        return p >= to ? 1.0 : 0.0;
    return clamp01((p - from) / (to - from));
}

QRectF logoRectFor(const QRectF &card)
{
    return QRectF(card.center().x() - kLogoSize / 2.0, card.top() + 52.0, kLogoSize, kLogoSize);
}

} // namespace

/* ------------------------------------------------------------------ */
/* 预设清单                                                            */
/* ------------------------------------------------------------------ */

QVector<SplashPresets::Item> SplashPresets::items()
{
    return {
        {QStringLiteral("swirl"), QStringLiteral("旋涡 · 旋转光环")},
        {QStringLiteral("particles"), QStringLiteral("星尘 · 粒子汇聚")},
        {QStringLiteral("aurora"), QStringLiteral("极光 · 流动光带")},
        {QStringLiteral("neon"), QStringLiteral("霓虹 · 描边点亮")},
        {QStringLiteral("shutter"), QStringLiteral("快门 · 切片合拢")},
        {QStringLiteral("orbit"), QStringLiteral("星轨 · 环绕光点")},
        {QStringLiteral("ripple"), QStringLiteral("涟漪 · 水波扩散")},
        {QStringLiteral("burst"), QStringLiteral("光爆 · 闪光辉映")},
        {QStringLiteral("flip3d"), QStringLiteral("3D 翻转 · 卡片翻面")},
        {QStringLiteral("levitate3d"), QStringLiteral("3D 悬浮 · 光台倒影")},
        {QStringLiteral("shatter3d"), QStringLiteral("3D 切片 · 立体拼合")},
        {QStringLiteral("fade"), QStringLiteral("淡入 · 柔光渐显")},
        {QStringLiteral("flash"), QStringLiteral("闪白 · 白闪出场")}
    };
}

QStringList SplashPresets::keys()
{
    QStringList list;
    for (const Item &item : items())
        list << item.key;
    return list;
}

QString SplashPresets::nameOf(const QString &key)
{
    for (const Item &item : items()) {
        if (item.key == key)
            return item.name;
    }
    return QStringLiteral("自定义动图");
}

/* ------------------------------------------------------------------ */
/* 窗口                                                                */
/* ------------------------------------------------------------------ */

SplashWindow::SplashWindow(QWidget *parent)
    : QWidget(parent, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(kCardWidth, kCardHeight);
    setCursor(Qt::PointingHandCursor);

    SettingsManager *sm = SettingsManager::instance();
    m_preset = sm->splashPreset();
    m_durationMs = qBound(1000, sm->splashDurationMs(), 10000);

    m_logo = AppIcon::logoPixmap(256);
    buildParticles();

    if (m_preset == QStringLiteral("custom")) {
        const QString path = sm->splashMediaPath();
        if (!path.isEmpty()) {
            if (isVideoFile(path)) {
                startVideo(path);
            } else {
                m_movie = new QMovie(path, QByteArray(), this);
                if (m_movie->isValid()) {
                    m_movie->setScaledSize(QSize(kCardWidth - 120, kCardHeight - 96));
                    m_movie->start();
                } else {
                    delete m_movie;
                    m_movie = nullptr;
                }
            }
        }
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, &SplashWindow::tick);
}

void SplashWindow::buildParticles()
{
    std::mt19937 rng(20261031u);
    std::uniform_real_distribution<qreal> angleDist(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<qreal> distDist(120.0, 330.0);
    std::uniform_real_distribution<qreal> sizeDist(1.2, 3.2);
    std::uniform_real_distribution<qreal> speedDist(0.7, 1.5);
    std::uniform_real_distribution<qreal> phaseDist(0.0, 1.0);

    m_particles.clear();
    m_particles.reserve(72);
    for (int i = 0; i < 72; ++i) {
        Particle p;
        p.angle = angleDist(rng);
        p.distance = distDist(rng);
        p.size = sizeDist(rng);
        p.speed = speedDist(rng);
        p.phase = phaseDist(rng);
        m_particles.append(p);
    }
}

void SplashWindow::start()
{
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        move(available.center() - rect().center());
    }
    setWindowOpacity(1.0);
    show();
    raise();
    m_elapsed.start();
    m_timer->start();
}

void SplashWindow::finishNow()
{
    if (m_finished)
        return;
    m_finished = true;
    m_timer->stop();
    if (m_movie)
        m_movie->stop();
    emit finished();
    close();
}

void SplashWindow::tick()
{
    const qreal elapsed = qreal(m_elapsed.elapsed());
    const qreal duration = qreal(m_durationMs);

    if (elapsed >= duration) {
        finishNow();
        return;
    }

    const qreal remaining = duration - elapsed;
    if (remaining < kFadeOutMs)
        setWindowOpacity(qMax<qreal>(0.0, remaining / kFadeOutMs));
    else
        setWindowOpacity(1.0);

    update();
}

/* ---------------- 自定义视频（MP4 / MOV / WebM） ---------------- */

void SplashWindow::startVideo(const QString &path)
{
#ifdef CJI_HAS_MULTIMEDIA
    m_videoSink = new QVideoSink(this);
    m_player = new QMediaPlayer(this);
    m_player->setVideoSink(m_videoSink);
    m_player->setLoops(QMediaPlayer::Infinite);
    m_player->setAudioOutput(nullptr);   // 启动画面不播音轨，音效走 AppSound

    connect(m_videoSink, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) {
                const QImage image = frame.toImage();
                if (image.isNull())
                    return;
                // 只保留到 2x 窗口大小，别让 4K 视频每帧都缩一次
                const QSize limit(width() * 2, height() * 2);
                m_videoFrame = image.size().width() > limit.width()
                                   ? image.scaled(limit, Qt::KeepAspectRatioByExpanding,
                                                  Qt::SmoothTransformation)
                                   : image;
                update();
            });

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::InvalidMedia) {
                    m_isVideo = false;
                    m_videoFrame = QImage();
                    update();
                }
            });

    m_isVideo = true;
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();
#else
    Q_UNUSED(path)
    m_isVideo = false;
#endif
}

void SplashWindow::drawVideo(QPainter &painter, const QRectF &card, qreal progress)
{
    if (m_videoFrame.isNull())
        return;   // 首帧还没解出来时，就保持卡片底色

    QPainterPath clip;
    clip = Squircle::path(card, 18);
    painter.save();
    painter.setClipPath(clip);

    // 铺满卡片（cover），不变形
    const QSizeF src = m_videoFrame.size();
    const qreal scale = qMax(card.width() / src.width(), card.height() / src.height());
    const QSizeF drawn(src.width() * scale, src.height() * scale);
    const QRectF dest(card.center().x() - drawn.width() / 2.0,
                      card.center().y() - drawn.height() / 2.0,
                      drawn.width(), drawn.height());

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setOpacity(clamp01(progress / 0.12));
    painter.drawImage(dest, m_videoFrame);
    painter.setOpacity(1.0);

    // 底部压暗一点，标题和进度条才看得清
    QLinearGradient scrim(card.bottomLeft(), QPointF(card.left(), card.bottom() - 120));
    scrim.setColorAt(0.0, QColor(0, 0, 0, 150));
    scrim.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(QRectF(card.left(), card.bottom() - 120, card.width(), 120), scrim);

    painter.restore();
}

/* ---------------- 公共绘制 ---------------- */

void SplashWindow::paintGlow(QPainter &painter, const QPointF &center, qreal radius,
                             const QColor &color, qreal alpha) const
{
    if (alpha <= 0.001 || radius <= 1.0)
        return;

    QRadialGradient gradient(center, radius);
    QColor inner = color;
    inner.setAlphaF(clamp01(alpha));
    QColor mid = color;
    mid.setAlphaF(clamp01(alpha) * 0.28);
    QColor outer = color;
    outer.setAlphaF(0.0);
    gradient.setColorAt(0.0, inner);
    gradient.setColorAt(0.55, mid);
    gradient.setColorAt(1.0, outer);

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(center, radius, radius);
}

void SplashWindow::paintCard(QPainter &painter, const QRectF &card, qreal progress) const
{
    Q_UNUSED(progress)

    QPainterPath path;
    path = Squircle::path(card, 18);

    // 底色：上下微微渐变，做出体积感
    QLinearGradient base(card.topLeft(), card.bottomLeft());
    base.setColorAt(0.0, QColor(16, 20, 31, 250));
    base.setColorAt(0.55, QColor(11, 15, 23, 250));
    base.setColorAt(1.0, QColor(8, 11, 18, 250));
    painter.fillPath(path, base);

    painter.save();
    painter.setClipPath(path);

    // 左上角蓝色环境光 + 右下角一点冷色反光
    paintGlow(painter, QPointF(card.left() + card.width() * 0.22, card.top() + 4),
              card.width() * 0.62, QColor(59, 130, 246), 0.20);
    paintGlow(painter, QPointF(card.right() - card.width() * 0.16, card.bottom() + 12),
              card.width() * 0.55, QColor(37, 99, 235), 0.10);

    // 顶部一层玻璃高光
    QLinearGradient topLight(card.topLeft(), QPointF(card.left(), card.top() + 96));
    topLight.setColorAt(0.0, QColor(255, 255, 255, 22));
    topLight.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillRect(QRectF(card.left(), card.top(), card.width(), 96), topLight);

    painter.restore();

    // 内侧 1px 高光边 + 外侧细描边
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 255, 255, 16), 1.0));
    painter.drawRoundedRect(card.adjusted(1, 1, -1, -1), 17, 17);
    painter.setPen(QPen(QColor(59, 130, 246, 58), 1.0));
    painter.drawPath(path);
}

void SplashWindow::paintLogoShadow(QPainter &painter, const QRectF &target) const
{
    // logo 下方的柔和投影，让它"立"在卡片上
    const QPointF center = target.center();
    const QPointF anchor = center + QPointF(0, target.height() * 0.62);
    QRadialGradient shadow(anchor, target.width() * 0.66);
    shadow.setColorAt(0.0, QColor(0, 0, 0, 88));
    shadow.setColorAt(0.55, QColor(0, 0, 0, 34));
    shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadow);
    painter.drawEllipse(anchor, target.width() * 0.50, target.height() * 0.17);
}

void SplashWindow::paintLogo(QPainter &painter, const QRectF &target, qreal progress,
                             qreal scaleFrom, qreal alpha) const
{
    const qreal appear = easeOutBack(seg(progress, 0.0, 0.42));
    const qreal scale = scaleFrom + (1.0 - scaleFrom) * appear;
    const QPointF center = target.center();
    const qreal opacity = clamp01(appear) * clamp01(alpha);

    if (opacity <= 0.001)
        return;

    paintLogoShadow(painter, target);

    painter.save();
    painter.translate(center);
    painter.scale(scale, scale);
    painter.translate(-center);
    painter.setOpacity(opacity);

    // 缩放一次后缓存，避免每帧重新生成
    const int wanted = int(target.width());
    if (m_logoScaled.isNull() || m_logoScaledSize != wanted) {
        m_logoScaled = m_logo.scaled(wanted, wanted, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);
        m_logoScaledSize = wanted;
    }
    painter.drawPixmap(target.toRect(), m_logoScaled);

    // 斜向扫光：只在高光扫过的那一小段时间画
    const qreal sweep = seg(progress, 0.32, 0.66);
    if (sweep > 0.0 && sweep < 1.0) {
        painter.save();
        QPainterPath clip;
        clip.addRoundedRect(target, target.width() * 0.23, target.width() * 0.23);
        painter.setClipPath(clip);

        const qreal x = target.left() - target.width() * 0.5 + sweep * target.width() * 1.9;
        QLinearGradient sheen(QPointF(x, target.top()),
                              QPointF(x + target.width() * 0.55, target.bottom()));
        sheen.setColorAt(0.0, QColor(255, 255, 255, 0));
        sheen.setColorAt(0.5, QColor(255, 255, 255, 96));
        sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillRect(target, sheen);
        painter.restore();
    }

    painter.restore();
}

void SplashWindow::paintText(QPainter &painter, const QRectF &card, qreal progress) const
{
    const qreal alpha = clamp01((progress - 0.16) / 0.34);
    if (alpha <= 0.001)
        return;

    const QRectF titleRect(card.left(), card.top() + 184, card.width(), 30);
    const QRectF subRect(card.left(), card.top() + 216, card.width(), 20);

    QFont titleFont = font();
    titleFont.setPixelSize(22);
    titleFont.setBold(true);
    titleFont.setFamily(QStringLiteral("Microsoft YaHei UI"));
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
    painter.setFont(titleFont);

    // 先画一层柔和投影，再画本体
    painter.setPen(QColor(0, 0, 0, int(110 * alpha)));
    painter.drawText(titleRect.translated(0, 1.5), Qt::AlignCenter, QStringLiteral("KOPAL"));
    painter.setPen(QColor(255, 255, 255, int(255 * alpha)));
    painter.drawText(titleRect, Qt::AlignCenter, QStringLiteral("KOPAL"));

    QFont subFont = font();
    subFont.setPixelSize(12);
    subFont.setFamily(QStringLiteral("Microsoft YaHei UI"));
    subFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
    painter.setFont(subFont);
    painter.setPen(QColor(150, 157, 170, int(215 * alpha)));
    painter.drawText(subRect, Qt::AlignCenter,
                     QString("%1  ·  正在准备备份界面").arg(Cji::versionLabel()));

    painter.setPen(QColor(110, 118, 132, int(150 * alpha)));
    painter.drawText(QRectF(card.left(), card.bottom() - 26, card.width() - 18, 16),
                     Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("点击任意处跳过"));
}

void SplashWindow::paintProgress(QPainter &painter, const QRectF &card, qreal progress) const
{
    const QRectF track(card.left() + 64, card.bottom() - 44, card.width() - 128, 3);
    QPainterPath trackPath;
    trackPath.addRoundedRect(track, 1.5, 1.5);
    painter.fillPath(trackPath, QColor(255, 255, 255, 18));

    const qreal filled = clamp01(progress);
    const QRectF fill(track.left(), track.top(), qMax<qreal>(2.0, track.width() * filled), 3);
    QPainterPath fillPath;
    fillPath.addRoundedRect(fill, 1.5, 1.5);

    QLinearGradient gradient(fill.topLeft(), fill.topRight());
    gradient.setColorAt(0.0, QColor(96, 165, 250));
    gradient.setColorAt(1.0, QColor(59, 130, 246));
    painter.fillPath(fillPath, gradient);

    // 一道顺着进度扫过的高光
    painter.save();
    painter.setClipPath(fillPath);
    const qreal shine = std::fmod(progress * 2.6 + 0.1, 1.0);
    const qreal sx = fill.left() + fill.width() * shine;
    QLinearGradient sheen(QPointF(sx - 46, fill.top()), QPointF(sx + 46, fill.top()));
    sheen.setColorAt(0.0, QColor(255, 255, 255, 0));
    sheen.setColorAt(0.5, QColor(255, 255, 255, 120));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillRect(fill, sheen);
    painter.restore();

    // 进度头部的小亮点
    if (filled > 0.02) {
        const QPointF head(fill.right(), fill.center().y());
        paintGlow(painter, head, 10.0, QColor(147, 197, 253), 0.55);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(219, 234, 254, 220));
        painter.drawEllipse(head, 1.6, 1.6);
    }
}
/* ---------------- 1. 旋涡 ---------------- */

void SplashWindow::paintSwirl(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const QPointF center = logo.center();
    const qreal appear = easeOutCubic(seg(progress, 0.0, 0.5));
    const qreal fade = 1.0 - clamp01((progress - 0.72) / 0.28);
    const qreal spin = progress * 900.0;

    paintGlow(painter, center, 108.0 + 22.0 * appear, QColor(59, 130, 246), 0.32 * fade);

    painter.save();
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < 4; ++i) {
        const qreal radius = 58.0 + i * 8.0 + 4.0 * appear;
        const qreal dir = (i % 2 == 0) ? 1.0 : -1.0;
        QConicalGradient gradient(center, spin * dir + i * 60.0);
        gradient.setColorAt(0.0, QColor(96, 165, 250, int(230 * fade)));
        gradient.setColorAt(0.28, QColor(59, 130, 246, int(40 * fade)));
        gradient.setColorAt(0.45, QColor(59, 130, 246, 0));
        gradient.setColorAt(1.0, QColor(59, 130, 246, 0));

        QPen pen(QBrush(gradient), 2.4 + i * 0.5);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        const qreal d = radius * 2.0;
        painter.drawArc(QRectF(center.x() - radius, center.y() - radius, d, d),
                        int((spin * dir + i * 90.0) * 16), int(120 * 16));
    }
    painter.restore();

    paintLogo(painter, logo, progress, 0.62);
}

/* ---------------- 2. 星尘汇聚 ---------------- */

void SplashWindow::paintParticles(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const QPointF center = logo.center();

    const qreal gather = easeInOutCubic(seg(progress, 0.0, 0.55));
    const qreal settle = 1.0 - clamp01((progress - 0.55) / 0.45);

    paintGlow(painter, center, 40.0 + 130.0 * gather, QColor(96, 165, 250),
              0.16 + 0.30 * gather);

    painter.save();
    painter.setPen(Qt::NoPen);
    for (const Particle &p : m_particles) {
        const qreal eased = easeInCubic(clamp01(gather * p.speed));
        const qreal distance = p.distance * (1.0 - eased);
        const qreal angle = p.angle + (1.0 - eased) * 2.2;
        const QPointF pos(center.x() + qCos(angle) * distance,
                          center.y() + qSin(angle) * distance * 0.82);

        const qreal twinkle = 0.55 + 0.45 * qSin(progress * 22.0 + p.phase * 8.0);
        const qreal alpha = clamp01(0.85 * twinkle * (0.35 + 0.65 * settle));

        painter.setBrush(QColor(140, 190, 255, int(220 * alpha)));
        const qreal size = p.size * (0.7 + 0.5 * settle);
        painter.drawEllipse(pos, size, size);

        painter.setBrush(QColor(96, 165, 250, int(70 * alpha)));
        painter.drawEllipse(pos, size * 2.4, size * 2.4);
    }
    painter.restore();

    paintLogo(painter, logo, progress, 0.55);
}

/* ---------------- 3. 极光 ---------------- */

void SplashWindow::paintAurora(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);

    painter.save();
    painter.setClipRect(card.adjusted(2, 2, -2, -2));
    painter.setBrush(Qt::NoBrush);

    const struct
    {
        QColor color;
        qreal phase;
        qreal amplitude;
        qreal thickness;
        qreal baseY;
    } ribbons[3] = {
        {QColor(59, 130, 246), 0.0, 26.0, 34.0, 78.0},
        {QColor(34, 211, 238), 1.9, 18.0, 26.0, 118.0},
        {QColor(167, 139, 250), 3.7, 22.0, 20.0, 96.0}};

    for (const auto &ribbon : ribbons) {
        const qreal drift = progress * 6.0 + ribbon.phase;
        QPainterPath path;
        path.moveTo(card.left() - 10, card.top() + ribbon.baseY);
        for (qreal x = 0; x <= card.width() + 20; x += 8.0) {
            const qreal wave = qSin(x / 58.0 + drift) * ribbon.amplitude
                               + qSin(x / 23.0 + drift * 1.7) * 8.0;
            path.lineTo(card.left() + x, card.top() + ribbon.baseY + wave);
        }

        QLinearGradient gradient(card.topLeft(), card.bottomLeft());
        gradient.setColorAt(0.0, QColor(ribbon.color.red(), ribbon.color.green(),
                                        ribbon.color.blue(), 0));
        gradient.setColorAt(0.45, QColor(ribbon.color.red(), ribbon.color.green(),
                                         ribbon.color.blue(), 110));
        gradient.setColorAt(0.8, QColor(ribbon.color.red(), ribbon.color.green(),
                                        ribbon.color.blue(), 30));
        gradient.setColorAt(1.0, QColor(ribbon.color.red(), ribbon.color.green(),
                                        ribbon.color.blue(), 0));

        QPen bandPen(QBrush(gradient), ribbon.thickness);
        bandPen.setCapStyle(Qt::RoundCap);
        painter.setPen(bandPen);
        painter.drawPath(path);
    }
    painter.restore();

    const qreal glowAlpha = 0.18 + 0.10 * qSin(progress * 9.0);
    paintGlow(painter, logo.center(), 118.0, QColor(96, 165, 250), glowAlpha);
    paintLogo(painter, logo, progress, 0.8);
}

/* ---------------- 4. 霓虹描边 ---------------- */

void SplashWindow::paintNeon(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const QPointF center = logo.center();
    const QRectF outline = logo.adjusted(6, 6, -6, -6);

    const qreal draw = easeInOutCubic(seg(progress, 0.05, 0.62));
    const qreal flash = 1.0 - clamp01((progress - 0.62) / 0.25);

    paintGlow(painter, center, 128.0, QColor(59, 130, 246), 0.10 + 0.22 * draw);

    QPainterPath roundRect;
    roundRect.addRoundedRect(outline, 24, 24);

    const qreal perimeter = 2.0 * (outline.width() + outline.height()) + 4.0 * 24.0;
    const qreal visible = perimeter * draw;

    if (draw > 0.001) {
        // 沿轮廓采样，画出"正在点亮"的那一段
        const int samples = 120;
        QPolygonF trace;
        trace.reserve(samples + 1);
        for (int s = 0; s <= samples; ++s) {
            const qreal t = qBound<qreal>(0.0, draw * s / samples, 1.0);
            trace << roundRect.pointAtPercent(t);
        }

        painter.save();
        painter.setBrush(Qt::NoBrush);

        const struct
        {
            qreal width;
            int alpha;
        } passes[3] = {{13.0, 34}, {6.0, 100}, {2.2, 255}};

        for (const auto &pass : passes) {
            QPen pen(QColor(150, 200, 255, pass.alpha), pass.width);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawPolyline(trace);
        }

        // 笔头亮点
        if (draw < 0.995) {
            const QPointF tip = roundRect.pointAtPercent(draw);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(200, 225, 255, 40));
            painter.drawEllipse(tip, 9, 9);
            painter.setBrush(QColor(230, 240, 255, 230));
            painter.drawEllipse(tip, 3.2, 3.2);
        }
        painter.restore();
    }

    if (flash > 0.01) {
        painter.save();
        painter.setOpacity(flash * 0.55);
        painter.setPen(QPen(QColor(200, 225, 255, 200), 1.6));
        painter.setBrush(QColor(120, 180, 255, 40));
        painter.drawRoundedRect(outline, 24, 24);
        painter.restore();
    }

    if (progress > 0.55)
        paintLogo(painter, logo, (progress - 0.55) / 0.45 * 0.42, 0.9);
}

/* ---------------- 5. 快门切片 ---------------- */

void SplashWindow::paintShutter(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const int slices = 9;
    const qreal sliceHeight = logo.height() / slices;

    for (int i = 0; i < slices; ++i) {
        const qreal open = easeOutCubic(seg(progress, 0.08 + i * 0.035, 0.34 + i * 0.035));
        if (open <= 0.001)
            continue;

        const QRectF slice(logo.left(), logo.top() + i * sliceHeight, logo.width(), sliceHeight);

        painter.save();
        painter.setClipRect(QRectF(slice.left(), slice.top(), slice.width(),
                                   slice.height() * open));
        paintLogo(painter, logo, 0.42, 1.0, 1.0);
        painter.restore();

        if (open < 0.995) {
            const qreal edgeY = slice.top() + slice.height() * open;
            QLinearGradient line(logo.left(), edgeY, logo.right(), edgeY);
            line.setColorAt(0.0, QColor(96, 165, 250, 0));
            line.setColorAt(0.5, QColor(147, 197, 253, 210));
            line.setColorAt(1.0, QColor(96, 165, 250, 0));
            painter.setPen(QPen(QBrush(line), 1.4));
            painter.drawLine(QPointF(logo.left(), edgeY), QPointF(logo.right(), edgeY));
        }
    }

    if (progress < 0.65) {
        const qreal y = logo.top() + logo.height() * easeInOutCubic(seg(progress, 0.0, 0.65));
        painter.setPen(QPen(QColor(96, 165, 250, 120), 1));
        painter.drawLine(QPointF(card.left() + 40, y), QPointF(card.right() - 40, y));
    }
}

/* ---------------- 6. 星轨环绕 ---------------- */

void SplashWindow::paintOrbit(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const QPointF center = logo.center();

    paintGlow(painter, center, 112.0, QColor(59, 130, 246), 0.18 + 0.10 * qSin(progress * 8.0));

    painter.save();
    painter.setBrush(Qt::NoBrush);

    const struct
    {
        qreal rx;
        qreal ry;
        qreal tilt;
        QColor color;
        qreal speed;
    } orbits[2] = {{104.0, 40.0, -18.0, QColor(96, 165, 250), 1.0},
                   {82.0, 32.0, 22.0, QColor(34, 211, 238), -1.35}};

    for (const auto &orbit : orbits) {
        painter.save();
        painter.translate(center);
        painter.rotate(orbit.tilt);

        painter.setPen(QPen(QColor(orbit.color.red(), orbit.color.green(),
                                   orbit.color.blue(), 45), 1.0));
        painter.drawEllipse(QPointF(0, 0), orbit.rx, orbit.ry);

        for (int dot = 0; dot < 2; ++dot) {
            const qreal angle = (progress * 6.2 * orbit.speed) + dot * M_PI;
            const QPointF pos(qCos(angle) * orbit.rx, qSin(angle) * orbit.ry);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(orbit.color.red(), orbit.color.green(),
                                    orbit.color.blue(), 60));
            painter.drawEllipse(pos, 7, 7);
            painter.setBrush(orbit.color);
            painter.drawEllipse(pos, 3, 3);
        }
        painter.restore();
    }
    painter.restore();

    paintLogo(painter, logo, progress, 0.7);
}

/* ---------------- 7. 水波扩散 ---------------- */

void SplashWindow::paintRipple(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const QPointF center = logo.center();

    painter.save();
    painter.setBrush(Qt::NoBrush);
    for (int i = 0; i < 5; ++i) {
        const qreal local = seg(progress, i * 0.08, 0.55 + i * 0.08);
        if (local <= 0.001 || local >= 1.0)
            continue;

        const qreal radius = 24.0 + local * 210.0;
        const qreal alpha = (1.0 - local) * 0.55;
        painter.setPen(QPen(QColor(96, 165, 250, int(255 * alpha)), 1.8 - local * 0.9));
        painter.drawEllipse(center, radius, radius * 0.68);
    }
    painter.restore();

    paintGlow(painter, center, 100.0, QColor(96, 165, 250), 0.22);
    paintLogo(painter, logo, seg(progress, 0.18, 0.75), 0.86);
}

/* ---------------- 8. 光爆 ---------------- */

void SplashWindow::paintBurst(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const QPointF center = logo.center();

    const qreal flash = 1.0 - clamp01(seg(progress, 0.02, 0.42));
    if (flash > 0.001)
        paintGlow(painter, center, 60.0 + 190.0 * easeOutCubic(1.0 - flash),
                  QColor(147, 197, 253), flash * 0.55);

    painter.save();
    painter.translate(center);
    painter.rotate(progress * 60.0);
    const int rays = 18;
    for (int i = 0; i < rays; ++i) {
        const qreal angle = i * (360.0 / rays);
        const qreal len = 58.0 + 26.0 * (0.5 + 0.5 * qSin(progress * 14.0 + i));
        painter.save();
        painter.rotate(angle);
        QLinearGradient line(0, 0, len, 0);
        line.setColorAt(0.0, QColor(165, 205, 255, 130));
        line.setColorAt(1.0, QColor(147, 197, 253, 0));
        painter.setPen(QPen(QBrush(line), 1.6));
        painter.drawLine(QPointF(50, 0), QPointF(len, 0));
        painter.restore();
    }
    painter.restore();

    const qreal ring = easeOutCubic(seg(progress, 0.05, 0.7));
    if (ring > 0.001 && ring < 1.0) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(191, 219, 254, int(180 * (1.0 - ring))), 2.0));
        const qreal r = 60.0 + ring * 150.0;
        painter.drawEllipse(center, r, r);
    }

    paintLogo(painter, logo, seg(progress, 0.1, 0.55), 0.5);
}

/* ---------------- 9. 淡入 ---------------- */

void SplashWindow::paintFade(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const qreal alpha = easeInOutCubic(seg(progress, 0.02, 0.6));

    paintGlow(painter, logo.center(), 118.0, QColor(59, 130, 246), 0.20 * alpha);

    painter.save();
    painter.setOpacity(alpha);
    const QRectF moved = logo.translated(0, 12.0 * (1.0 - alpha));
    painter.drawPixmap(moved.toRect(), AppIcon::logoPixmap(int(moved.width())));
    painter.restore();
}

/* ---------------- 10. 闪白 ---------------- */

void SplashWindow::paintFlash(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);

    const qreal white = 1.0 - clamp01(seg(progress, 0.0, 0.22));
    if (white > 0.001) {
        painter.save();
        painter.setOpacity(white);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255));
        painter.drawRoundedRect(card, 18, 18);
        painter.restore();
    }

    const qreal bloom = seg(progress, 0.05, 0.55);
    if (bloom > 0.001 && bloom < 1.0) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(191, 219, 254, int(200 * (1.0 - bloom))), 3.0));
        const qreal r = 40.0 + easeOutCubic(bloom) * 170.0;
        painter.drawEllipse(logo.center(), r, r);
    }

    if (progress > 0.18)
        paintLogo(painter, logo, (progress - 0.18) / 0.82, 0.55);
}

/* ---------------- 11. 3D 翻转 ---------------- */

void SplashWindow::paintFlip3D(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const QPointF center = logo.center();

    const qreal t = seg(progress, 0.04, 0.62);
    const qreal eased = easeOutBack(t);
    const qreal rotY = -88.0 * (1.0 - eased);
    const qreal rotX = 8.0 * (1.0 - eased);
    const qreal alpha = clamp01(t * 3.0);

    paintGlow(painter, center, 118.0, QColor(59, 130, 246), 0.26 * alpha);
    Splash3d::drawContactShadow(painter, center + QPointF(-rotY * 0.35, 54), 60, 10, 0.35 * alpha);

    // 翻完之后继续做一点悬浮摆动，3D 感一直在
    const qreal settle = easeOutCubic(seg(progress, 0.62, 0.85));
    const qreal time = progress * 7.0;
    const qreal liveRotX = rotX + 7.0 * qSin(time * 1.1) * settle;
    const qreal liveRotY = rotY + 12.0 * qSin(time * 0.8 + 0.6) * settle;

    const QPolygonF quad = Splash3d::projectQuad(logo, liveRotX, liveRotY);
    Splash3d::drawPixmap3D(painter, m_logo, logo, liveRotX, liveRotY, 1.0, alpha);

    if (t > 0.55)
        Splash3d::drawSpecular(painter, quad, seg(progress, 0.55, 0.95));

    // 侧向的时候勾一道边光，正面就不画了
    if (qAbs(rotY) > 5.0) {
        painter.setPen(QPen(QColor(147, 197, 253, int(150 * qMin<qreal>(1.0, qAbs(rotY) / 40.0))), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(quad);
    }
}

/* ---------------- 12. 3D 悬浮 ---------------- */

void SplashWindow::paintLevitate3D(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const qreal appear = easeOutCubic(seg(progress, 0.02, 0.45));
    const qreal time = progress * 7.0;

    const qreal rotX = 12.0 * qSin(time * 0.9) * appear;
    const qreal rotY = 22.0 * qSin(time * 0.68 + 1.1) * appear;
    const qreal hover = 5.0 * qSin(time * 1.15) * appear;

    const QRectF target = logo.translated(0, hover);
    const QPointF center = target.center();

    paintGlow(painter, center, 128.0, QColor(96, 165, 250),
              0.18 + 0.10 * qSin(progress * 9.0));

    // 光台
    painter.save();
    painter.setPen(Qt::NoPen);
    QRadialGradient stage(center + QPointF(0, 60), 96);
    stage.setColorAt(0.0, QColor(96, 165, 250, int(80 * appear)));
    stage.setColorAt(1.0, QColor(96, 165, 250, 0));
    painter.setBrush(stage);
    painter.drawEllipse(center + QPointF(0, 58), 84, 14);
    painter.restore();

    // 台面光斑 + 投影
    painter.save();
    painter.setPen(Qt::NoPen);
    QRadialGradient pool(center + QPointF(-rotY * 0.3, 62), 72);
    pool.setColorAt(0.0, QColor(96, 165, 250, int(110 * appear)));
    pool.setColorAt(0.7, QColor(59, 130, 246, int(45 * appear)));
    pool.setColorAt(1.0, QColor(59, 130, 246, 0));
    painter.setBrush(pool);
    painter.drawEllipse(center + QPointF(-rotY * 0.3, 62), 74, 12);
    painter.restore();

    Splash3d::drawContactShadow(painter, center + QPointF(-rotY * 0.30, 60), 64, 11, 0.55 * appear);

    const QPolygonF quad = Splash3d::projectQuad(target, rotX, rotY);
    Splash3d::drawPixmap3D(painter, m_logo, target, rotX, rotY, 1.0, appear);
    Splash3d::drawSpecular(painter, quad, 0.2 + 0.6 * progress, QColor(170, 205, 255));
}

/* ---------------- 13. 3D 切片 ---------------- */

void SplashWindow::paintShatter3D(QPainter &painter, const QRectF &card, qreal progress)
{
    const QRectF logo = logoRectFor(card);
    const int slices = 8;
    const qreal sliceW = logo.width() / slices;
    const qreal srcW = m_logo.width() / qreal(slices);

    paintGlow(painter, logo.center(), 120.0, QColor(59, 130, 246), 0.22);

    // 拼好之后整体轻轻侧转，让 3D 一直在线
    const qreal settle = easeOutCubic(seg(progress, 0.5, 0.75));
    const qreal time = progress * 7.0;
    const qreal liveRotX = 6.0 * qSin(time * 1.05) * settle;
    const qreal liveRotY = 13.0 * qSin(time * 0.82 + 0.4) * settle;

    for (int i = 0; i < slices; ++i) {
        const qreal t = seg(progress, 0.02 + i * 0.022, 0.26 + i * 0.022);
        if (t <= 0.001)
            continue;

        const qreal eased = easeOutCubic(t);
        const qreal dir = (i % 2 == 0) ? -1.0 : 1.0;
        const qreal rotY = dir * 45.0 * (1.0 - eased);
        const qreal rotX = 10.0 * (1.0 - eased);

        // 稍微重叠一点，避免缝隙露底
        const QRectF target(logo.left() + i * sliceW, logo.top(), sliceW + 0.6, logo.height());
        const QRectF source(i * srcW, 0, qMin(srcW + 1.0, m_logo.width() - i * srcW),
                            m_logo.height());

        Splash3d::drawPixmapSlice3D(painter, m_logo, source, target, rotX + liveRotX,
                                    rotY + liveRotY, 1.0, clamp01(t * 2.2));

        if (t < 0.98) {
            const qreal x = target.left();
            QLinearGradient line(x, logo.top(), x, logo.bottom());
            line.setColorAt(0.0, QColor(147, 197, 253, 0));
            line.setColorAt(0.5, QColor(191, 219, 254, int(190 * (1.0 - t))));
            line.setColorAt(1.0, QColor(147, 197, 253, 0));
            painter.setPen(QPen(QBrush(line), 1.2));
            painter.drawLine(QPointF(x, logo.top()), QPointF(x, logo.bottom()));
        }
    }
}

/* ---------------- 绘制入口 ---------------- */

void SplashWindow::paintEvent(QPaintEvent *)
{
    const qreal progress = clamp01(qreal(m_elapsed.elapsed()) / qreal(m_durationMs));
    const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    ++m_frameIndex;

    // 1) 先用 2 倍分辨率离屏渲染，缩放下来边缘才干净（相当于超采样）
    const int ss = 2;
    const QSize frameSize(width() * ss, height() * ss);
    if (m_frame.size() != frameSize)
        m_frame = QImage(frameSize, QImage::Format_ARGB32_Premultiplied);
    m_frame.fill(Qt::transparent);
    {
        QPainter scene(&m_frame);
        scene.setRenderHint(QPainter::Antialiasing, true);
        scene.setRenderHint(QPainter::SmoothPixmapTransform, true);
        scene.scale(ss, ss);
        drawScene(scene, card, progress);
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(rect(), m_frame);

    // 2) 泛光：缩到 1/4 再放大回来叠加，模拟 AE 的 Glow
    const QSize bloomSize(qMax(1, width() / 6), qMax(1, height() / 6));
    if (m_bloom.size() != bloomSize)
        m_bloom = QImage(bloomSize, QImage::Format_ARGB32_Premultiplied);
    m_bloom.fill(Qt::transparent);
    {
        QPainter glow(&m_bloom);
        glow.setRenderHint(QPainter::SmoothPixmapTransform, true);
        glow.drawImage(QRect(QPoint(0, 0), bloomSize), m_frame);
    }
    painter.setCompositionMode(QPainter::CompositionMode_Plus);
    painter.setOpacity(0.16);
    painter.drawImage(rect(), m_bloom);
    painter.setOpacity(1.0);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // 3) 暗角 + 胶片颗粒
    drawPostEffects(painter, card);
}

void SplashWindow::drawScene(QPainter &painter, const QRectF &card, qreal progress)
{
    paintCard(painter, card, progress);

    if (m_preset == QStringLiteral("particles")) {
        paintParticles(painter, card, progress);
    } else if (m_preset == QStringLiteral("aurora")) {
        paintAurora(painter, card, progress);
    } else if (m_preset == QStringLiteral("neon")) {
        paintNeon(painter, card, progress);
    } else if (m_preset == QStringLiteral("shutter")) {
        paintShutter(painter, card, progress);
    } else if (m_preset == QStringLiteral("orbit")) {
        paintOrbit(painter, card, progress);
    } else if (m_preset == QStringLiteral("ripple")) {
        paintRipple(painter, card, progress);
    } else if (m_preset == QStringLiteral("burst")) {
        paintBurst(painter, card, progress);
    } else if (m_preset == QStringLiteral("flip3d")) {
        paintFlip3D(painter, card, progress);
    } else if (m_preset == QStringLiteral("levitate3d")) {
        paintLevitate3D(painter, card, progress);
    } else if (m_preset == QStringLiteral("shatter3d")) {
        paintShatter3D(painter, card, progress);
    } else if (m_preset == QStringLiteral("fade")) {
        paintFade(painter, card, progress);
    } else if (m_preset == QStringLiteral("flash")) {
        paintFlash(painter, card, progress);
    } else if (m_preset == QStringLiteral("custom") && m_isVideo) {
        drawVideo(painter, card, progress);
    } else if (m_preset == QStringLiteral("custom") && m_movie) {
        const QRectF logo = logoRectFor(card);
        const QPixmap frame = m_movie->currentPixmap();
        if (!frame.isNull()) {
            const QSize scaled = frame.size().scaled(
                QSize(int(logo.width() * 2.4), int(card.height() - 110)), Qt::KeepAspectRatio);
            const QRectF target(card.center().x() - scaled.width() / 2.0, card.top() + 26,
                                scaled.width(), scaled.height());
            painter.setOpacity(clamp01(progress / 0.2));
            painter.drawPixmap(target.toRect(), frame);
            painter.setOpacity(1.0);
        }
    } else {
        paintSwirl(painter, card, progress);
    }

    paintText(painter, card, progress);
    paintProgress(painter, card, progress);
}

void SplashWindow::drawPostEffects(QPainter &painter, const QRectF &card)
{
    ensureGrain();

    QPainterPath path;
    path = Squircle::path(card, 18);
    painter.save();
    painter.setClipPath(path);

    // 暗角：四周压暗，中间保持亮
    QRadialGradient vignette(card.center(), card.width() * 0.74);
    vignette.setColorAt(0.52, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 120));
    painter.fillRect(card, vignette);

    // 胶片颗粒：每帧轻微位移，避免看着是"静态贴图"
    if (!m_grain.isNull()) {
        painter.setOpacity(0.05);
        const int ox = (m_frameIndex * 37) % m_grain.width();
        const int oy = (m_frameIndex * 53) % m_grain.height();
        QBrush brush(m_grain);
        QTransform shift;
        shift.translate(ox, oy);
        brush.setTransform(shift);
        painter.fillRect(card, brush);
        painter.setOpacity(1.0);
    }

    painter.restore();
}

void SplashWindow::ensureGrain()
{
    if (!m_grain.isNull())
        return;

    const int size = 96;
    m_grain = QImage(size, size, QImage::Format_ARGB32);
    m_grain.fill(Qt::transparent);

    std::mt19937 rng(20261101u);
    std::uniform_int_distribution<int> dist(0, 255);
    for (int y = 0; y < size; ++y) {
        auto *line = reinterpret_cast<QRgb *>(m_grain.scanLine(y));
        for (int x = 0; x < size; ++x) {
            const int v = dist(rng);
            line[x] = qRgba(v, v, v, 18);
        }
    }
}
void SplashWindow::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    finishNow();
}

void SplashWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space)
        finishNow();
    QWidget::keyPressEvent(event);
}
