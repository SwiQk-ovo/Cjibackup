#include "core/thumbnailloader.h"
#include "core/perfmonitor.h"
#include "core/perfprofile.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMediaPlayer>
#include <QRunnable>
#include <QStandardPaths>
#include <QDebug>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

namespace {
constexpr int kMaxSide = 400;
constexpr int kJpegQuality = 70;
constexpr int kProbeTimeoutMs = 12000;

QString cacheDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString dir = base + QStringLiteral("/thumbnails");
    QDir().mkpath(dir);
    return dir;
}

QString cacheKey(const QString &path)
{
    const QFileInfo info(path);
    const QString seed = QStringLiteral("%1|%2|%3")
                             .arg(QDir::toNativeSeparators(info.absoluteFilePath()))
                             .arg(info.lastModified().toMSecsSinceEpoch())
                             .arg(info.size());
    return QString::fromLatin1(
        QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha1).toHex());
}

bool isVideoFile(const QString &path)
{
    static const QSet<QString> exts = {
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("m4v"),
        QStringLiteral("mkv"), QStringLiteral("avi"), QStringLiteral("wmv"),
        QStringLiteral("webm"), QStringLiteral("mts"), QStringLiteral("m2ts"),
        QStringLiteral("3gp"), QStringLiteral("mpg"), QStringLiteral("mpeg"),
    };
    return exts.contains(QFileInfo(path).suffix().toLower());
}

// RAW（CR2/NEF/ARW/DNG/ORF/RW2/PEF/RAF）里一般嵌了一张 JPEG 预览，抽出来当缩略图
QImage extractEmbeddedJpeg(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const qint64 limit = qMin<qint64>(file.size(), 48 * 1024 * 1024);
    const QByteArray data = file.read(limit);
    if (data.isEmpty())
        return {};
    const QByteArray soi = QByteArray::fromHex("ffd8ff");
    const QByteArray eoi = QByteArray::fromHex("ffd9");
    int from = 0;
    while (true) {
        const int start = data.indexOf(soi, from);
        if (start < 0)
            break;
        const int end = data.indexOf(eoi, start + 3);
        if (end < 0)
            break;
        const QByteArray jpeg = data.mid(start, end - start + 2);
        QImage image = QImage::fromData(jpeg);
        if (!image.isNull() && image.width() >= 160)
            return image;
        from = end + 2;
    }
    return {};
}
QImage shrink(const QImage &src)
{
    if (src.isNull())
        return src;
    if (src.width() <= kMaxSide && src.height() <= kMaxSide)
        return src;
    return src.scaled(kMaxSide, kMaxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

// 后台线程：解码照片
class ImageJob : public QRunnable
{
public:
    ImageJob(const QString &path, const QString &cacheFile)
        : m_path(path), m_cache(cacheFile) {}

    void run() override
    {
        QImageReader reader(m_path);
        reader.setAutoTransform(true);
        QSize size = reader.size();
        if (size.isValid()) {
            size.scale(QSize(kMaxSide, kMaxSide), Qt::KeepAspectRatio);
            if (size.width() > 0 && size.height() > 0)
                reader.setScaledSize(size);
        }
        QImage image = reader.read();
        if (image.isNull())
            image = extractEmbeddedJpeg(m_path);   // RAW：抽内嵌预览
        if (image.isNull()) {
            // RAW / 少见的格式读不了，就给个空图，界面显示占位
            emitDone(QImage());
            return;
        }
        image = shrink(image);
        image.save(m_cache, "JPG", kJpegQuality);
        emitDone(image);
    }

    std::function<void(const QImage &)> onDone;
    void emitDone(const QImage &image)
    {
        QImage copy = image;
        if (onDone)
            onDone(copy);
    }

private:
    QString m_path;
    QString m_cache;
};
} // namespace

ThumbnailLoader::ThumbnailLoader(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(qMax(2, QThread::idealThreadCount() / 2));
    cacheDir();
}

ThumbnailLoader &ThumbnailLoader::instance()
{
    static ThumbnailLoader loader;
    return loader;
}

QString ThumbnailLoader::cacheFileFor(const QString &filePath)
{
    return cacheDir() + QLatin1Char('/') + cacheKey(filePath) + QStringLiteral(".jpg");
}

void ThumbnailLoader::request(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    const QString cache = cacheFileFor(filePath);
    if (m_done.value(filePath) == cache && QFileInfo::exists(cache)) {
        QImage image(cache);
        if (!image.isNull()) {
            emit ready(filePath, image);
            return;
        }
    }
    if (QFileInfo::exists(cache)) {
        QImage image(cache);
        if (!image.isNull()) {
            m_done.insert(filePath, cache);
            emit ready(filePath, image);
            return;
        }
    }
    if (m_pending.contains(filePath))
        return;

    m_pending.insert(filePath);
    if (isVideoFile(filePath)) {
        m_videoQueue.append(filePath);
        pumpVideoQueue();
    } else {
        startImageJob(filePath);
    }
}

void ThumbnailLoader::requestBatch(const QStringList &filePaths)
{
    for (const QString &path : filePaths)
        request(path);
}

void ThumbnailLoader::startImageJob(const QString &path)
{
    const QString cache = cacheFileFor(path);
    auto *job = new ImageJob(path, cache);
    job->onDone = [this, path, cache](const QImage &image) {
        QMetaObject::invokeMethod(this, [this, path, cache, image]() {
            m_pending.remove(path);
            if (!image.isNull()) {
                m_done.insert(path, cache);
                emit ready(path, image);
            }
        }, Qt::QueuedConnection);
    };
    job->setAutoDelete(true);
    m_pool.start(job);
}

bool ThumbnailLoader::startVideoProbe(const QString &path)
{
    const int slotCount = qMax(1, PerfProfile::instance().thumbProbeSlots()
                                  / (PerfMonitor::instance().wallDegraded() ? 2 : 1));
    int slot = -1;
    for (int i = 0; i < slotCount; ++i) {
        if (m_probes[i].path.isEmpty()) { slot = i; break; }
    }
    if (slot < 0)
        return false;

    // 复用播放器：每 new 一个 QMediaPlayer 都要初始化一次 FFmpeg 后端，
    // 一屏十几条视频就会变成十几秒的卡顿
    QMediaPlayer *player = m_probes[slot].player;
    QVideoSink *sink = m_probes[slot].sink;
    if (!player) {
        player = new QMediaPlayer(this);
        sink = new QVideoSink(player);
        player->setVideoSink(sink);
        m_probes[slot].player = player;
        m_probes[slot].sink = sink;
        connect(sink, &QVideoSink::videoFrameChanged, this,
                [this, slot](const QVideoFrame &frame) {
                    const QImage image = frame.toImage();
                    if (image.isNull() || m_probes[slot].path.isEmpty())
                        return;
                    finishVideoProbe(m_probes[slot].path, image);
                });
    }
    m_probes[slot].path = path;

    player->setSource(QUrl::fromLocalFile(path));
    player->play();
    QTimer::singleShot(120, this, [this, path]() {
        for (auto &probe : m_probes) {
            if (probe.path == path && probe.player) {
                probe.player->setPosition(80);
                probe.player->play();
            }
        }
    });
    QTimer::singleShot(kProbeTimeoutMs, this, [this, path]() {
        finishVideoProbe(path, QImage());
    });
    return true;
}

void ThumbnailLoader::releaseProbe(const QString &path)
{
    for (auto &probe : m_probes) {
        if (probe.path != path || !probe.player)
            continue;
        probe.player->stop();
        probe.player->setSource(QUrl());
        probe.path.clear();
    }
}

void ThumbnailLoader::finishVideoProbe(const QString &path, const QImage &frame)
{
    bool active = false;
    for (const auto &probe : m_probes) {
        if (probe.player && probe.path == path)
            active = true;
    }
    if (!active)
        return;

    releaseProbe(path);
    m_pending.remove(path);

    if (!frame.isNull()) {
        const QImage scaled = shrink(frame);
        const QString cache = cacheFileFor(path);
        scaled.save(cache, "JPG", kJpegQuality);
        m_done.insert(path, cache);
        emit ready(path, scaled);
    }
    QTimer::singleShot(0, this, [this]() { pumpVideoQueue(); });
}

void ThumbnailLoader::pumpVideoQueue()
{
    for (;;) {
        if (m_videoQueue.isEmpty())
            return;
        const QString next = m_videoQueue.takeFirst();
        // 排队超过 3 秒还没轮到，说明用户已经滚过去了，别再浪费解码
        const qint64 queuedAt = m_enqueuedAt.value(next, 0);
        if (queuedAt > 0 && QDateTime::currentMSecsSinceEpoch() - queuedAt > 3000) {
            m_pending.remove(next);
            m_enqueuedAt.remove(next);
            continue;
        }
        m_enqueuedAt.remove(next);
        if (!startVideoProbe(next)) {
            m_videoQueue.prepend(next);
            return;   // 两个槽都占着，等有空位再继续
        }
    }
}