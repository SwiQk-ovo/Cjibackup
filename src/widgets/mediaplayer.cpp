#include "widgets/mediaplayer.h"

#include "core/perfmonitor.h"
#include "core/perfprofile.h"
#include "widgets/toast.h"

#include <QApplication>
#include <QAudioOutput>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QLibrary>
#include <QMediaPlayer>

#ifdef CJI_HAS_VIDEO_WIDGET
#include <QVideoWidget>
#endif
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

#include <functional>

namespace {

const QColor kBg(0x0E, 0x0F, 0x11);
const QColor kCard(0x17, 0x18, 0x1B);
const QColor kAccent(0x4C, 0x8D, 0xFF);
const QColor kText(0xFF, 0xFF, 0xFF);
const QColor kMuted(0x9A, 0xA0, 0xA8);
const QColor kTrack(0x3A, 0x40, 0x50);

const QSet<QString> kImageExts = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
    QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
    QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("heic"),
    QStringLiteral("cr2"), QStringLiteral("cr3"), QStringLiteral("nef"),
    QStringLiteral("arw"), QStringLiteral("dng"), QStringLiteral("orf"),
    QStringLiteral("rw2"), QStringLiteral("raf"), QStringLiteral("pef"),
};

QString formatTime(qint64 ms)
{
    if (ms < 0)
        ms = 0;
    const qint64 total = ms / 1000;
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    if (h > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

} // namespace

// 自绘视频画面：QVideoSink 送帧，这里画到窗口（不依赖 MultimediaWidgets）
class VideoSurface : public QWidget
{
public:
    explicit VideoSurface(QWidget *parent = nullptr) : QWidget(parent)
    {
        setStyleSheet(QStringLiteral("background:#000000;"));
        setAttribute(Qt::WA_OpaquePaintEvent, true);
    }
    void setImage(const QImage &image) { m_image = image; m_cacheKey = QSize(); update(); }
    QImage image() const { return m_image; }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QElapsedTimer frameClock;
        frameClock.start();

        QPainter p(this);
        p.fillRect(rect(), Qt::black);
        if (m_image.isNull())
            return;

        // 第二层动态调整：画面缩放走缓存，掉帧时换成快速变换 + 半分辨率
        const bool fast = PerfMonitor::instance().videoFastScale();
        const QSize target = fast ? QSize(qMax(1, width() / 2), qMax(1, height() / 2)) : size();
        if (m_cacheKey != target || m_scaled.isNull()) {
            m_scaled = m_image.scaled(target, Qt::KeepAspectRatio,
                                      fast ? Qt::FastTransformation : Qt::SmoothTransformation);
            m_cacheKey = target;
        }
        const QRect dst((width() - m_scaled.width() * (fast ? 2 : 1)) / 2,
                        (height() - m_scaled.height() * (fast ? 2 : 1)) / 2,
                        m_scaled.width() * (fast ? 2 : 1),
                        m_scaled.height() * (fast ? 2 : 1));
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(dst, m_scaled);

        // 记录上屏耗时作为掉帧依据
        PerfMonitor::instance().reportVideoFrame(int(frameClock.elapsed()) + 1);
    }

private:
    QImage m_image;
    QImage m_scaled;
    QSize m_cacheKey;
};
// ============================================================
class ControlBar : public QWidget
{
public:
    explicit ControlBar(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMouseTracking(true);
        setFixedHeight(48);
    }

    std::function<void()> onPlayPause;
    std::function<void()> onPrev;
    std::function<void()> onNext;
    std::function<void()> onSpeed;
    std::function<void()> onFullScreen;
    std::function<void(qint64)> onSeek;
    std::function<void(int)> onVolume;
    std::function<QImage()> onBackdrop;   // 背后画面（视频当前帧）

    void setState(bool playing, qint64 pos, qint64 dur, qreal rate, int volume)
    {
        m_playing = playing;
        m_pos = pos;
        m_dur = dur;
        m_rate = rate;
        m_volume = volume;
        update();
    }

    void setHoverVisible(bool visible)
    {
        m_visibleTarget = visible;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        // 控制条：实心底 + 一条分隔线，不用毛玻璃/发光
        p.fillRect(rect(), QColor(0x0E, 0x0F, 0x11, 235));
        p.setPen(QPen(QColor(255, 255, 255, 18), 1));
        p.drawLine(0, 0, width(), 0);

        m_rects.clear();
        int x = 14;
        const int cy = height() / 2;

        auto button = [&](const QString &id, int w) {
            QRect r(x, cy - 16, w, 32);
            m_rects.insert(id, r);
            x += w + 6;
            return r;
        };

        // 播放 / 暂停
        const QRect playRect = button(QStringLiteral("play"), 32);
        p.setPen(Qt::NoPen);
        p.setBrush(kText);
        if (m_playing) {
            p.drawRoundedRect(QRectF(playRect.center().x() - 6, cy - 9, 4, 18), 1, 1);
            p.drawRoundedRect(QRectF(playRect.center().x() + 2, cy - 9, 4, 18), 1, 1);
        } else {
            QPainterPath tri;
            tri.moveTo(playRect.center().x() - 5, cy - 9);
            tri.lineTo(playRect.center().x() - 5, cy + 9);
            tri.lineTo(playRect.center().x() + 9, cy);
            tri.closeSubpath();
            p.drawPath(tri);
        }

        // 上一个 / 下一个
        const QRect prevRect = button(QStringLiteral("prev"), 28);
        const QRect nextRect = button(QStringLiteral("next"), 28);
        p.setBrush(kMuted);
        QPainterPath prevTri, nextTri;
        prevTri.moveTo(prevRect.center().x() + 6, cy - 7);
        prevTri.lineTo(prevRect.center().x() + 6, cy + 7);
        prevTri.lineTo(prevRect.center().x() - 6, cy);
        prevTri.closeSubpath();
        p.drawPath(prevTri);
        nextTri.moveTo(nextRect.center().x() - 6, cy - 7);
        nextTri.lineTo(nextRect.center().x() - 6, cy + 7);
        nextTri.lineTo(nextRect.center().x() + 6, cy);
        nextTri.closeSubpath();
        p.drawPath(nextTri);

        // 时间
        QFont mono = font();
        mono.setFamily(QStringLiteral("Consolas"));
        mono.setPixelSize(12);
        p.setFont(mono);
        p.setPen(kText);
        const QString timeText = formatTime(m_pos) + QStringLiteral(" / ") + formatTime(m_dur);
        const int timeW = QFontMetrics(mono).horizontalAdvance(timeText) + 18;
        const QRect timeRect(x, cy - 12, timeW, 24);
        p.drawText(timeRect, Qt::AlignLeft | Qt::AlignVCenter, timeText);
        x += timeW;

        // 进度条
        const int rightReserve = 250;
        const int barLeft = x;
        const int barRight = width() - rightReserve;
        const QRect track(barLeft, cy - 2, qMax(40, barRight - barLeft), 4);
        m_trackRect = track;
        p.setPen(Qt::NoPen);
        p.setBrush(kTrack);
        p.drawRoundedRect(track, 2, 2);
        qreal ratio = m_dur > 0 ? qreal(m_pos) / qreal(m_dur) : 0.0;
        ratio = qBound<qreal>(0, ratio, 1);
        QRect filled(track.left(), track.top(), int(track.width() * ratio), track.height());
        p.setBrush(kAccent);
        p.drawRoundedRect(filled, 2, 2);
        p.setBrush(QColor(0xFF, 0xFF, 0xFF));
        p.drawEllipse(QPointF(filled.right(), track.center().y()), 5, 5);

        // 右侧：音量 / 倍速 / 全屏
        int rx = width() - 14;

        const QRect fsRect(rx - 28, cy - 16, 28, 32);
        m_rects.insert(QStringLiteral("fullscreen"), fsRect);
        rx -= 34;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kText, 1.6));
        const int c = 7;
        p.drawLine(fsRect.center().x() - c, cy - c, fsRect.center().x() - c + 5, cy - c);
        p.drawLine(fsRect.center().x() - c, cy - c, fsRect.center().x() - c, cy - c + 5);
        p.drawLine(fsRect.center().x() + c, cy - c, fsRect.center().x() + c - 5, cy - c);
        p.drawLine(fsRect.center().x() + c, cy - c, fsRect.center().x() + c, cy - c + 5);
        p.drawLine(fsRect.center().x() - c, cy + c, fsRect.center().x() - c + 5, cy + c);
        p.drawLine(fsRect.center().x() - c, cy + c, fsRect.center().x() - c, cy + c - 5);
        p.drawLine(fsRect.center().x() + c, cy + c, fsRect.center().x() + c - 5, cy + c);
        p.drawLine(fsRect.center().x() + c, cy + c, fsRect.center().x() + c, cy + c - 5);

        QFont small = font();
        small.setPixelSize(12);
        p.setFont(small);
        const QString rateText = QString::number(m_rate, 'g', 3) + QStringLiteral("x");
        const int rateW = QFontMetrics(small).horizontalAdvance(rateText) + 16;
        const QRect rateRect(rx - rateW, cy - 14, rateW, 28);
        m_rects.insert(QStringLiteral("speed"), rateRect);
        p.setPen(kMuted);
        p.drawText(rateRect, Qt::AlignCenter, rateText);
        rx -= rateW + 10;

        // 音量
        const QRect volIcon(rx - 22, cy - 16, 22, 32);
        m_rects.insert(QStringLiteral("volumeIcon"), volIcon);
        p.setPen(Qt::NoPen);
        p.setBrush(kMuted);
        QPainterPath spk;
        spk.moveTo(volIcon.left() + 4, cy - 4);
        spk.lineTo(volIcon.left() + 8, cy - 4);
        spk.lineTo(volIcon.left() + 13, cy - 9);
        spk.lineTo(volIcon.left() + 13, cy + 9);
        spk.lineTo(volIcon.left() + 8, cy + 4);
        spk.lineTo(volIcon.left() + 4, cy + 4);
        spk.closeSubpath();
        p.drawPath(spk);
        rx -= 26;

        const QRect volTrack(rx - 70, cy - 2, 70, 4);
        m_volumeRect = volTrack;
        p.setBrush(kTrack);
        p.drawRoundedRect(volTrack, 2, 2);
        const qreal volRatio = qBound<qreal>(0, m_volume / 100.0, 1);
        p.setBrush(kAccent);
        p.drawRoundedRect(QRect(volTrack.left(), volTrack.top(), int(volTrack.width() * volRatio), 4), 2, 2);

        if (m_hovered) {
            p.setPen(QPen(QColor(0x4C, 0x8D, 0xFF, 120), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(rect().adjusted(0, -60, 0, 0), 0, 0);
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        const QPoint pos = e->pos();
        if (m_trackRect.adjusted(-4, -8, 4, 8).contains(pos)) {
            m_dragSeek = true;
            emitSeek(pos.x());
            return;
        }
        if (m_volumeRect.adjusted(-4, -8, 4, 8).contains(pos)) {
            m_dragVolume = true;
            emitVolume(pos.x());
            return;
        }
        for (auto it = m_rects.constBegin(); it != m_rects.constEnd(); ++it) {
            if (it.value().contains(pos)) {
                if (it.key() == QStringLiteral("play") && onPlayPause)
                    onPlayPause();
                else if (it.key() == QStringLiteral("prev") && onPrev)
                    onPrev();
                else if (it.key() == QStringLiteral("next") && onNext)
                    onNext();
                else if (it.key() == QStringLiteral("speed") && onSpeed)
                    onSpeed();
                else if (it.key() == QStringLiteral("fullscreen") && onFullScreen)
                    onFullScreen();
                return;
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_dragSeek)
            emitSeek(e->pos().x());
        else if (m_dragVolume)
            emitVolume(e->pos().x());
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        m_dragSeek = false;
        m_dragVolume = false;
    }

    void enterEvent(QEnterEvent *) override
    {
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent *) override
    {
        m_hovered = false;
        update();
    }

private:
    void emitSeek(int x)
    {
        if (!onSeek || m_trackRect.width() <= 0 || m_dur <= 0)
            return;
        qreal ratio = qreal(x - m_trackRect.left()) / qreal(m_trackRect.width());
        ratio = qBound<qreal>(0, ratio, 1);
        onSeek(qint64(ratio * m_dur));
    }

    void emitVolume(int x)
    {
        if (!onVolume || m_volumeRect.width() <= 0)
            return;
        qreal ratio = qreal(x - m_volumeRect.left()) / qreal(m_volumeRect.width());
        ratio = qBound<qreal>(0, ratio, 1);
        onVolume(int(ratio * 100));
    }

    bool m_playing = false;
    bool m_hovered = false;
    bool m_visibleTarget = true;
    bool m_dragSeek = false;
    bool m_dragVolume = false;
    qint64 m_pos = 0;
    qint64 m_dur = 0;
    qreal m_rate = 1.0;
    int m_volume = 80;
    QRect m_trackRect;
    QRect m_volumeRect;
    QHash<QString, QRect> m_rects;
};

// ============================================================
MediaPlayerWindow::MediaPlayerWindow(const QString &path, QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    setWindowTitle(QFileInfo(path).fileName());
    resize(1180, 720);
    setStyleSheet(QStringLiteral("QWidget{background:#000000;color:#FFFFFF;}"));

    buildUi();
    m_playlist = {path};
    m_index = 0;
    openPath(path);

    m_timer = new QTimer(this);
    m_timer->setInterval(200);
    connect(m_timer, &QTimer::timeout, this, &MediaPlayerWindow::onTick);
    m_timer->start();

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(2500);
    connect(m_hideTimer, &QTimer::timeout, this, [this]() {
        if (m_bar && !m_bar->underMouse())
            m_bar->setVisible(false);
    });

    setMouseTracking(true);
}

MediaPlayerWindow::~MediaPlayerWindow()
{
    shutdownMpv();
    if (m_player)
        m_player->stop();
}

void MediaPlayerWindow::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *contentRow = new QWidget(this);
    auto *contentLayout = new QHBoxLayout(contentRow);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    layout->addWidget(contentRow, 1);

    m_videoArea = new QWidget(contentRow);
    m_videoArea->setStyleSheet(QStringLiteral("background:#000000;"));
    m_videoArea->installEventFilter(this);
    m_videoArea->setMouseTracking(true);
    contentLayout->addWidget(m_videoArea, 1);

    // 右侧播放列表：只在有多条视频时出现
    m_list = new QListWidget(contentRow);
    m_list->setObjectName(QStringLiteral("playlist"));
    m_list->setFixedWidth(240);
    m_list->setStyleSheet(QStringLiteral(
        "QListWidget#playlist{background:#0E0E0F;border:none;outline:none;padding:8px;}"
        "QListWidget#playlist::item{color:#B9B9BD;padding:8px 10px;border-radius:8px;}"
        "QListWidget#playlist::item:hover{background:#1B1B1D;color:#FFFFFF;}"
        "QListWidget#playlist::item:selected{background:#262628;color:#FFFFFF;}"));
    m_list->hide();
    contentLayout->addWidget(m_list);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        const int row = m_list->row(item);
        if (row < 0 || row >= m_playlist.size() || row == m_index)
            return;
        if (m_player)
            m_player->stop();
        m_index = row;
        openPath(m_playlist.at(m_index));
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const int row = m_list->row(item);
        if (row < 0 || row >= m_playlist.size() || row == m_index)
            return;
        if (m_player)
            m_player->stop();
        m_index = row;
        openPath(m_playlist.at(m_index));
    });

    auto *videoLayout = new QVBoxLayout(m_videoArea);
    videoLayout->setContentsMargins(0, 0, 0, 0);

    m_imageLabel = new QLabel(m_videoArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(QStringLiteral("background:#000000;"));
    m_imageLabel->hide();
    videoLayout->addWidget(m_imageLabel, 1);

#ifdef CJI_HAS_VIDEO_WIDGET
    // 硬件渲染路径：视频帧直接交给 QVideoWidget，不做逐帧 QImage 转换
    auto *vout = new QVideoWidget(m_videoArea);
    vout->setStyleSheet(QStringLiteral("background:#000000;"));
    m_videoWidget = vout;
    videoLayout->addWidget(m_videoWidget, 1);
#else
    auto *surface = new VideoSurface(m_videoArea);
    m_videoWidget = surface;
    videoLayout->addWidget(m_videoWidget, 1);

    // 视频控件本身也要收鼠标事件，否则控制条唤不出来
    m_videoWidget->installEventFilter(this);
    m_videoWidget->setMouseTracking(true);
    m_videoWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);

    m_sink = new QVideoSink(this);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this, surface](const QVideoFrame &frame) {
        const QImage image = frame.toImage();
        if (!image.isNull()) {
            surface->setImage(image);
            m_frame = image;
        }
    });
#endif

    m_hint = new QLabel(this);
    m_hint->setStyleSheet(QStringLiteral("color:#9C9C9F;font-size:12px;background:transparent;"));
    m_hint->move(16, 12);
    m_hint->hide();

    m_bar = new ControlBar(this);
    m_bar->onPlayPause = [this]() { togglePlay(); };
    m_bar->onPrev = [this]() { playRelative(-1); };
    m_bar->onNext = [this]() { playRelative(1); };
    m_bar->onSpeed = [this]() { cycleRate(); };
    m_bar->onFullScreen = [this]() { toggleFullScreen(); };
    m_bar->onSeek = [this](qint64 ms) {
        if (m_useMpv)
            mpvCommand({QStringLiteral("seek"), QString::number(ms / 1000.0, 'f', 3), QStringLiteral("absolute")});
        else if (m_player)
            m_player->setPosition(ms);
    };
    m_bar->onVolume = [this](int v) { setVolume(v); };
    m_bar->onBackdrop = [this]() -> QImage { return m_frame; };
    m_bar->raise();
}

void MediaPlayerWindow::setPlaylist(const QStringList &files, int current)
{
    if (files.isEmpty())
        return;
    m_playlist = files;
    m_index = qBound(0, current, files.size() - 1);
}

void MediaPlayerWindow::openPath(const QString &path)
{
    m_path = path;
    setWindowTitle(QFileInfo(path).fileName());
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (kImageExts.contains(suffix)) {
        openImage(path);
    } else {
        openVideo(path);
    }
    updateFromBackend();
}

void MediaPlayerWindow::openImage(const QString &path)
{
    m_isImage = true;
    m_duration = 0;
    m_position = 0;
    if (m_player)
        m_player->stop();
    m_videoWidget->hide();

    QImage image(path);
    if (image.isNull()) {
        m_imageLabel->setText(QStringLiteral("这个格式暂时读不了（RAW 预览后续再加）"));
        m_imageLabel->setStyleSheet(QStringLiteral("color:#9C9C9F;background:#000000;"));
        m_imageLabel->show();
        return;
    }
    m_imageLabel->setStyleSheet(QStringLiteral("background:#000000;"));
    m_imageLabel->setPixmap(QPixmap::fromImage(image).scaled(
        m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imageLabel->show();
}

void MediaPlayerWindow::openVideo(const QString &path)
{
    m_isImage = false;
    m_imageLabel->hide();
    m_videoWidget->show();

    if (initMpv()) {
        m_useMpv = true;
        m_videoWidget->hide();
        mpvCommand({QStringLiteral("loadfile"), QDir::toNativeSeparators(path)});
        mpvCommand({QStringLiteral("set"), QStringLiteral("volume"), QString::number(m_volume)});
        mpvCommand({QStringLiteral("set"), QStringLiteral("pause"), QStringLiteral("no")});
        m_playing = true;
        showHint(QStringLiteral("mpv 内核 · 硬解优先"));
        return;
    }

    m_useMpv = false;
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_audio = new QAudioOutput(this);
        m_player->setAudioOutput(m_audio);
    #ifdef CJI_HAS_VIDEO_WIDGET
    if (auto *vout = qobject_cast<QVideoWidget *>(m_videoWidget))
        m_player->setVideoOutput(vout);
    else
        m_player->setVideoSink(m_sink);
#else
    m_player->setVideoSink(m_sink);
#endif
        m_audio->setVolume(m_volume / 100.0);
        connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 d) {
            m_duration = d;
        });
        connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 p) {
            m_position = p;
        });
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::InvalidMedia) {
                showHint(QStringLiteral("这个编码解不了，可以试试安装 libmpv"));
                Toast::showToast(QStringLiteral("无法播放"),
                                 QStringLiteral("当前解码器不支持这个文件"), Toast::Warning, 2600);
            }
        });
    }
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->play();
    m_playing = true;
}

bool MediaPlayerWindow::initMpv()
{
    if (m_mpv)
        return true;
    if (qEnvironmentVariableIsSet("KOPAL_NO_MPV"))
        return false;

    const QStringList candidates = {QStringLiteral("mpv-2"), QStringLiteral("libmpv-2"),
                                    QStringLiteral("mpv-1")};
    for (const QString &name : candidates) {
        auto *lib = new QLibrary(name, this);
        if (!lib->load())
            continue;
        using CreateFn = void *(*)();
        using InitFn = int (*)(void *);
        using SetOptFn = int (*)(void *, const char *, const char *);
        using CmdFn = int (*)(void *, const char **);
        auto create = reinterpret_cast<CreateFn>(lib->resolve("mpv_create"));
        auto init = reinterpret_cast<InitFn>(lib->resolve("mpv_initialize"));
        auto setOpt = reinterpret_cast<SetOptFn>(lib->resolve("mpv_set_option_string"));
        auto command = reinterpret_cast<CmdFn>(lib->resolve("mpv_command"));
        if (!create || !init || !setOpt || !command) {
            lib->unload();
            delete lib;
            continue;
        }
        void *handle = create();
        if (!handle) {
            lib->unload();
            delete lib;
            continue;
        }
        m_mpvLib = lib;
        m_mpv = handle;
        // 按本机性能档位下发解码参数（后台自动，不打扰用户）
        const PerfProfile &perf = PerfProfile::instance();
        setOpt(handle, "hwdec", perf.mpvHwdec().constData());
        setOpt(handle, "vo", perf.mpvVo().constData());
        setOpt(handle, "keep-open", "yes");
        setOpt(handle, "cache", "yes");
        setOpt(handle, "cache-secs", perf.mpvCacheSecs().constData());
        setOpt(handle, "demuxer-max-bytes", perf.mpvDemuxerMax().constData());
        setOpt(handle, "vd-lavc-threads", QByteArray::number(perf.mpvThreads()).constData());
        if (perf.tier() == PerfProfile::Lite)
            setOpt(handle, "vd-lavc-fast", "yes");   // 软解时跳过部分后处理
        if (init(handle) < 0) {
            shutdownMpv();
            return false;
        }
        const auto wid = reinterpret_cast<quintptr>(m_videoArea->winId());
        setOpt(handle, "wid", QByteArray::number(qint64(wid)).constData());
        return true;
    }
    return false;
}

void MediaPlayerWindow::mpvCommand(const QStringList &args)
{
    if (!m_mpv || !m_mpvLib)
        return;
    using CmdFn = int (*)(void *, const char **);
    auto command = reinterpret_cast<CmdFn>(static_cast<QLibrary *>(m_mpvLib)->resolve("mpv_command"));
    if (!command)
        return;
    QByteArray storage;
    QVector<const char *> argv;
    for (const QString &arg : args) {
        storage += arg.toUtf8();
        storage += '\0';
    }
    const char *p = storage.constData();
    while (p < storage.constData() + storage.size()) {
        argv.append(p);
        p += qstrlen(p) + 1;
    }
    argv.append(nullptr);
    const char **argvPtr = const_cast<const char **>(argv.constData());
    command(m_mpv, argvPtr);
}

void MediaPlayerWindow::shutdownMpv()
{
    if (!m_mpv || !m_mpvLib)
        return;
    using DestroyFn = void (*)(void *);
    auto destroy = reinterpret_cast<DestroyFn>(static_cast<QLibrary *>(m_mpvLib)->resolve("mpv_terminate_destroy"));
    if (destroy)
        destroy(m_mpv);
    m_mpv = nullptr;
    m_mpvLib = nullptr;
    m_useMpv = false;
}

void MediaPlayerWindow::togglePlay()
{
    if (m_isImage)
        return;
    m_playing = !m_playing;
    if (m_useMpv) {
        mpvCommand({QStringLiteral("set"), QStringLiteral("pause"),
                    m_playing ? QStringLiteral("no") : QStringLiteral("yes")});
    } else if (m_player) {
        if (m_playing)
            m_player->play();
        else
            m_player->pause();
    }
    updateFromBackend();
}

void MediaPlayerWindow::step(const int deltaSeconds)
{
    if (m_isImage)
        return;
    qint64 target = m_position + deltaSeconds * 1000;
    target = qBound<qint64>(0, target, qMax<qint64>(0, m_duration));
    if (m_useMpv)
        mpvCommand({QStringLiteral("seek"), QString::number(deltaSeconds), QStringLiteral("relative")});
    else if (m_player)
        m_player->setPosition(target);
    m_position = target;
    updateFromBackend();
}

void MediaPlayerWindow::stepFrame(int deltaFrames)
{
    if (m_isImage)
        return;
    const qint64 frameMs = 40;   // 25fps 近似
    step(int(deltaFrames * frameMs / 1000.0) * 0 + (deltaFrames > 0 ? 1 : -1));
    if (m_useMpv)
        mpvCommand({QStringLiteral("frame-step")});
}

void MediaPlayerWindow::applyRate(qreal rate)
{
    m_rate = rate;
    if (m_useMpv)
        mpvCommand({QStringLiteral("set"), QStringLiteral("speed"), QString::number(rate, 'f', 2)});
    else if (m_player)
        m_player->setPlaybackRate(rate);
    updateFromBackend();
}

void MediaPlayerWindow::cycleRate()
{
    static const qreal rates[] = {0.5, 1.0, 1.5, 2.0};
    int idx = 0;
    for (int i = 0; i < 4; ++i) {
        if (qFuzzyCompare(m_rate, rates[i])) {
            idx = (i + 1) % 4;
            break;
        }
    }
    applyRate(rates[idx]);
}

void MediaPlayerWindow::setVolume(int volume)
{
    m_volume = qBound(0, volume, 100);
    if (m_useMpv) {
        mpvCommand({QStringLiteral("set"), QStringLiteral("volume"), QString::number(m_volume)});
    } else if (m_audio) {
        m_audio->setVolume(m_volume / 100.0);
    }
    updateFromBackend();
}

void MediaPlayerWindow::toggleFullScreen()
{
    if (m_fullscreen) {
        showNormal();
        m_fullscreen = false;
    } else {
        showFullScreen();
        m_fullscreen = true;
    }
    m_bar->setVisible(true);
}

void MediaPlayerWindow::onTick()
{
    if (m_useMpv && m_mpv && m_mpvLib) {
        // 轮询位置/时长（mpv 属性读取是线程安全的）
        using GetPropFn = int (*)(void *, const char *, int, void *);
        auto getProp = reinterpret_cast<GetPropFn>(static_cast<QLibrary *>(m_mpvLib)->resolve("mpv_get_property"));
        if (getProp) {
            double pos = 0, dur = 0;
            if (getProp(m_mpv, "time-pos", 5 /*MPV_FORMAT_DOUBLE*/, &pos) >= 0)
                m_position = qint64(pos * 1000);
            if (getProp(m_mpv, "duration", 5, &dur) >= 0)
                m_duration = qint64(dur * 1000);
        }
    } else if (m_player) {
        m_position = m_player->position();
        m_duration = m_player->duration();
    }
    updateFromBackend();
}

void MediaPlayerWindow::updateFromBackend()
{
    if (m_bar)
        m_bar->setState(m_playing && !m_isImage, m_position, m_duration, m_rate, m_volume);
}

void MediaPlayerWindow::showHint(const QString &text)
{
    if (!m_hint)
        return;
    m_hint->setText(text);
    m_hint->adjustSize();
    m_hint->show();
    m_hint->raise();
    QTimer::singleShot(4000, m_hint, &QLabel::hide);
}

void MediaPlayerWindow::playRelative(int delta)
{
    if (m_playlist.size() < 2)
        return;
    const int next = qBound(0, m_index + delta, m_playlist.size() - 1);
    if (next == m_index)
        return;
    // 切歌前先彻底停掉旧的
    if (m_player)
        m_player->stop();
    m_index = next;
    openPath(m_playlist.at(m_index));
}

void MediaPlayerWindow::takeScreenshot()
{
    QImage frame;
    if (m_isImage) {
        frame = QImage(m_path);
    } else if (m_useMpv) {
        const QString dir = QApplication::applicationDirPath() + QStringLiteral("/screenshots");
        QDir().mkpath(dir);
        const QString file = dir + QStringLiteral("/%1.png")
                                       .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        mpvCommand({QStringLiteral("screenshot-to-file"), file, QStringLiteral("video")});
        Toast::showToast(QStringLiteral("已截图"), QDir::toNativeSeparators(file), Toast::Success, 2600);
        return;
    } else if (auto *surface = static_cast<VideoSurface *>(m_videoWidget)) {
        frame = surface->image();
    }

    if (frame.isNull()) {
        Toast::showToast(QStringLiteral("截图失败"), QStringLiteral("当前没有可用画面"),
                         Toast::Warning, 2200);
        return;
    }
    const QString dir = QApplication::applicationDirPath() + QStringLiteral("/screenshots");
    QDir().mkpath(dir);
    const QString file = dir + QStringLiteral("/%1.png")
                                   .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    if (frame.save(file)) {
        Toast::showToast(QStringLiteral("已截图"), QDir::toNativeSeparators(file), Toast::Success, 2600);
    }
}

void MediaPlayerWindow::showProperties()
{
    const QFileInfo info(m_path);
    QStringList lines;
    lines << QStringLiteral("文件：") + info.fileName();
    lines << QStringLiteral("路径：") + QDir::toNativeSeparators(info.absoluteFilePath());
    lines << QStringLiteral("大小：") + QString::number(info.size() / 1024.0 / 1024.0, 'f', 2) + QStringLiteral(" MB");
    lines << QStringLiteral("修改时间：") + info.lastModified().toString("yyyy-MM-dd HH:mm:ss");
    if (m_isImage) {
        const QImage image(m_path);
        if (!image.isNull())
            lines << QStringLiteral("尺寸：") + QStringLiteral("%1 × %2").arg(image.width()).arg(image.height());
    } else {
        lines << QStringLiteral("时长：") + formatTime(m_duration);
        lines << QStringLiteral("解码：") + (m_useMpv ? QStringLiteral("libmpv（硬解优先）")
                                                      : QStringLiteral("Qt Multimedia"));
    }
    Toast::showToast(QStringLiteral("素材信息"), lines.join(QStringLiteral("\n")),
                     Toast::Info, 6000);
}

void MediaPlayerWindow::showContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu{background:#1B1B1D;border:1px solid #2A3040;border-radius:10px;padding:6px;}"
        "QMenu::item{color:#FFFFFF;padding:7px 22px;border-radius:6px;font-size:13px;}"
        "QMenu::item:selected{background:#4C8DFF;}"));

    menu.addAction(m_playing ? QStringLiteral("暂停") : QStringLiteral("播放"), this, [this]() { togglePlay(); });
    menu.addAction(QStringLiteral("快进 10 秒"), this, [this]() { step(10); });
    menu.addAction(QStringLiteral("快退 10 秒"), this, [this]() { step(-10); });
    menu.addAction(QStringLiteral("逐帧步进"), this, [this]() { stepFrame(1); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("截图（保存当前帧）"), this, [this]() { takeScreenshot(); });
    menu.addAction(QStringLiteral("打开文件"), this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, QStringLiteral("打开素材"), QFileInfo(m_path).absolutePath(),
            QStringLiteral("媒体文件 (*.mp4 *.mov *.mkv *.jpg *.png *.jpeg)"));
        if (!file.isEmpty()) {
            m_playlist = {file};
            m_index = 0;
            openPath(file);
        }
    });
    menu.addAction(QStringLiteral("全屏"), this, [this]() { toggleFullScreen(); });
    menu.addSeparator();
    QMenu *speedMenu = menu.addMenu(QStringLiteral("倍速"));
    speedMenu->setStyleSheet(menu.styleSheet());
    for (qreal rate : {0.5, 1.0, 1.5, 2.0}) {
        speedMenu->addAction(QStringLiteral("%1x").arg(rate), this, [this, rate]() { applyRate(rate); });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("属性"), this, [this]() { showProperties(); });
    menu.exec(globalPos);
}

void MediaPlayerWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        togglePlay();
        return;
    case Qt::Key_Left:
        if (event->modifiers() & Qt::ShiftModifier)
            stepFrame(-1);
        else
            step(-3);
        return;
    case Qt::Key_Right:
        if (event->modifiers() & Qt::ShiftModifier)
            stepFrame(1);
        else
            step(3);
        return;
    case Qt::Key_Up:
        setVolume(m_volume + 5);
        return;
    case Qt::Key_Down:
        setVolume(m_volume - 5);
        return;
    case Qt::Key_Escape:
        if (m_fullscreen)
            toggleFullScreen();
        else
            close();
        return;
    case Qt::Key_F:
        toggleFullScreen();
        return;
    case Qt::Key_S:
        takeScreenshot();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void MediaPlayerWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_bar) {
        m_bar->setVisible(true);
        m_bar->move(0, height() - m_bar->height());
        m_bar->resize(width(), m_bar->height());
        m_bar->raise();
        if (m_hideTimer)
            m_hideTimer->start();
    }
    QWidget::mouseMoveEvent(event);
}

void MediaPlayerWindow::mouseDoubleClickEvent(QMouseEvent *)
{
    toggleFullScreen();
}

void MediaPlayerWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_bar) {
        m_bar->setGeometry(0, height() - m_bar->height(), width(), m_bar->height());
        m_bar->raise();
    }
    if (m_hint)
        m_hint->move(16, 12);
    if (m_isImage && m_imageLabel) {
        const QImage image(m_path);
        if (!image.isNull())
            m_imageLabel->setPixmap(QPixmap::fromImage(image).scaled(
                m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MediaPlayerWindow::closeEvent(QCloseEvent *event)
{
    if (m_player)
        m_player->stop();
    shutdownMpv();
    QWidget::closeEvent(event);
}

bool MediaPlayerWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_videoArea || watched == m_videoWidget) {
        if (event->type() == QEvent::MouseMove) {
            const auto *me = static_cast<QMouseEvent *>(event);
            mouseMoveEvent(const_cast<QMouseEvent *>(me));
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            toggleFullScreen();
            return true;
        } else if (event->type() == QEvent::ContextMenu) {
            const auto *ce = static_cast<QContextMenuEvent *>(event);
            showContextMenu(ce->globalPos());
            return true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            const auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::RightButton)
                return false;
        }
    }
    return QWidget::eventFilter(watched, event);
}
