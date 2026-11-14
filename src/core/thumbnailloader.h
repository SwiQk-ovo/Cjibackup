#ifndef THUMBNAILLOADER_H
#define THUMBNAILLOADER_H

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

class QMediaPlayer;
class QVideoSink;

/**
 * 异步缩略图加载 + 本地缓存。
 *
 * 照片：QImageReader 在后台线程解码，最长边压到 400px，JPEG 质量 70%。
 * 视频：QMediaPlayer + QVideoSink 抓第一帧（主线程串行，避免多实例抢解码器）。
 * 缓存：QStandardPaths::CacheLocation/thumbnails/<hash>.jpg，命中就不重新生成。
 */
class ThumbnailLoader : public QObject
{
    Q_OBJECT

public:
    static ThumbnailLoader &instance();

    // 缓存文件路径（不管存不存在）
    static QString cacheFileFor(const QString &filePath);

    // 请求缩略图；已有缓存会立刻发出 ready
    void request(const QString &filePath);

    // 预置：把某个目录下所有文件的缩略图排进队列（可选）
    void requestBatch(const QStringList &filePaths);

signals:
    void ready(const QString &filePath, const QImage &image);

private:
    explicit ThumbnailLoader(QObject *parent = nullptr);

    void startImageJob(const QString &path);
    bool startVideoProbe(const QString &path);
    void finishVideoProbe(const QString &path, const QImage &frame);
    void releaseProbe(const QString &path);
    void pumpVideoQueue();

    struct Probe {
        QMediaPlayer *player = nullptr;
        QVideoSink *sink = nullptr;
        QString path;
    };
    Probe m_probes[2];   // 两个并发探测，首帧出得更快

    QThreadPool m_pool;
    QSet<QString> m_pending;   // 正在生成
    QHash<QString, QString> m_done; // path -> cache file
    QList<QString> m_videoQueue;
    QHash<QString, qint64> m_enqueuedAt;
};

#endif // THUMBNAILLOADER_H
