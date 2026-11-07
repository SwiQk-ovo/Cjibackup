#ifndef SPLASHWINDOW_H
#define SPLASHWINDOW_H

#include <QElapsedTimer>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPixmap>
#include <QWidget>

class QMovie;
class QTimer;

#ifdef CJI_HAS_MULTIMEDIA
class QMediaPlayer;
class QVideoSink;
#endif

namespace SplashPresets {

struct Item
{
    QString key;
    QString name;
};

// 内置启动动画清单（13 个）
QVector<Item> items();
QStringList keys();
QString nameOf(const QString &key);

} // namespace SplashPresets

/**
 * 启动动画窗口：13 种内置风格 + 自定义动图 / 视频。
 * 播放结束后自动关闭并发 finished()；点击可跳过。
 */
class SplashWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SplashWindow(QWidget *parent = nullptr);

    void start();
    void finishNow();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Particle
    {
        qreal angle = 0;
        qreal distance = 0;
        qreal size = 1;
        qreal speed = 1;
        qreal phase = 0;
    };

    void tick();
    void buildParticles();
    void startVideo(const QString &path);
    void drawVideo(QPainter &painter, const QRectF &card, qreal progress);

    // 渲染管线：先画到 2x 离屏图，再缩放下来做泛光 / 暗角 / 颗粒
    void drawScene(QPainter &painter, const QRectF &card, qreal progress);
    void drawPostEffects(QPainter &painter, const QRectF &card);
    void ensureGrain();

    // 公共部件
    void paintCard(QPainter &painter, const QRectF &card, qreal progress) const;
    void paintLogo(QPainter &painter, const QRectF &target, qreal progress,
                   qreal scaleFrom = 0.72, qreal alpha = 1.0) const;
    void paintLogoShadow(QPainter &painter, const QRectF &target) const;
    void paintText(QPainter &painter, const QRectF &card, qreal progress) const;
    void paintProgress(QPainter &painter, const QRectF &card, qreal progress) const;
    void paintGlow(QPainter &painter, const QPointF &center, qreal radius,
                   const QColor &color, qreal alpha) const;

    // 13 种风格
    void paintSwirl(QPainter &painter, const QRectF &card, qreal progress);
    void paintFade(QPainter &painter, const QRectF &card, qreal progress);
    void paintFlash(QPainter &painter, const QRectF &card, qreal progress);
    void paintParticles(QPainter &painter, const QRectF &card, qreal progress);
    void paintAurora(QPainter &painter, const QRectF &card, qreal progress);
    void paintNeon(QPainter &painter, const QRectF &card, qreal progress);
    void paintShutter(QPainter &painter, const QRectF &card, qreal progress);
    void paintOrbit(QPainter &painter, const QRectF &card, qreal progress);
    void paintRipple(QPainter &painter, const QRectF &card, qreal progress);
    void paintBurst(QPainter &painter, const QRectF &card, qreal progress);
    void paintFlip3D(QPainter &painter, const QRectF &card, qreal progress);
    void paintLevitate3D(QPainter &painter, const QRectF &card, qreal progress);
    void paintShatter3D(QPainter &painter, const QRectF &card, qreal progress);

    QString m_preset;
    int m_durationMs = 2600;
    QElapsedTimer m_elapsed;
    QTimer *m_timer = nullptr;
    QMovie *m_movie = nullptr;
    bool m_finished = false;
    QVector<Particle> m_particles;
    QPixmap m_logo;
    mutable QPixmap m_logoScaled;
    mutable int m_logoScaledSize = 0;

    // 自定义视频（MP4 / MOV / WebM）播放
    bool m_isVideo = false;
    QImage m_videoFrame;
#ifdef CJI_HAS_MULTIMEDIA
    QMediaPlayer *m_player = nullptr;
    QVideoSink *m_videoSink = nullptr;
#endif

    QImage m_frame;      // 2x 离屏画面
    QImage m_bloom;      // 1/4 尺寸，用来做泛光
    QImage m_grain;      // 胶片颗粒
    int m_frameIndex = 0;
};

#endif // SPLASHWINDOW_H
