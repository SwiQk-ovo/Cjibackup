#include "pages/album_page.h"

#include "core/settingsmanager.h"
#include "core/perfmonitor.h"
#include "core/perfprofile.h"
#include "core/thumbnailloader.h"
#include "widgets/mediawall.h"
#include "widgets/glasspanel.h"
#include "widgets/mediaplayer.h"
#include "widgets/toast.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

const QSet<QString> kImageExts = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
    QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
    QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("heic"),
    QStringLiteral("cr2"), QStringLiteral("cr3"), QStringLiteral("nef"),
    QStringLiteral("arw"), QStringLiteral("dng"), QStringLiteral("orf"),
    QStringLiteral("rw2"), QStringLiteral("raf"), QStringLiteral("pef"),
};
const QSet<QString> kVideoExts = {
    QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("m4v"),
    QStringLiteral("mkv"), QStringLiteral("avi"), QStringLiteral("wmv"),
    QStringLiteral("webm"), QStringLiteral("mts"), QStringLiteral("m2ts"),
    QStringLiteral("3gp"), QStringLiteral("mpg"), QStringLiteral("mpeg"),
};

// 铺满目标区域再居中裁切：不变形，也没有黑边，看起来整齐
void drawCover(QPainter *p, const QRectF &target, const QImage &image)
{
    if (image.isNull() || target.isEmpty())
        return;
    QSizeF scaled = image.size();
    scaled.scale(target.size(), Qt::KeepAspectRatio);
    // 放大到能覆盖整个目标
    const qreal cover = qMax(target.width() / scaled.width(), target.height() / scaled.height());
    scaled *= cover;
    const QRectF src(0, 0, image.width(), image.height());
    const QRectF dst(target.center().x() - scaled.width() / 2,
                     target.center().y() - scaled.height() / 2,
                     scaled.width(), scaled.height());
    p->drawImage(dst, image, src);
}
QIcon viewModeIcon(int mode)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(18 * dpr), int(18 * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(0x9A, 0xA0, 0xA8));
    pen.setWidthF(1.4);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (mode == 0) {          // 网格
        const qreal s = 6.5, g = 2.5, x0 = 1.2, y0 = 1.2;
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 2; ++c)
                p.drawRoundedRect(QRectF(x0 + c * (s + g), y0 + r * (s + g), s, s), 1.6, 1.6);
    } else if (mode == 1) {   // 列表
        for (int r = 0; r < 3; ++r) {
            const qreal y = 3 + r * 5.5;
            p.drawRoundedRect(QRectF(1.2, y, 4, 3.6), 1.2, 1.2);
            p.drawLine(QPointF(7.5, y + 1.8), QPointF(16.5, y + 1.8));
        }
    } else if (mode == 3) {   // 刷新：圆环缺口在右上，箭头对齐圆心
        p.drawArc(QRectF(3.5, 3.5, 11, 11), 60 * 16, 285 * 16);
        QPainterPath head;
        head.moveTo(12.4, 2.4);
        head.lineTo(15.0, 6.2);
        head.lineTo(10.8, 6.6);
        head.closeSubpath();
        p.setBrush(QColor(0x9A, 0xA0, 0xA8));
        p.setPen(Qt::NoPen);
        p.drawPath(head);
    } else if (mode == 4) {   // 上一级：顶横线 + 向上的箭头，整体居中
        p.setBrush(Qt::NoBrush);
        p.setPen(pen);
        p.drawLine(QPointF(4.2, 4.2), QPointF(13.8, 4.2));
        p.drawLine(QPointF(9, 14.6), QPointF(9, 7.2));
        QPainterPath up;
        up.moveTo(5.6, 9.4);
        up.lineTo(9, 6.0);
        up.lineTo(12.4, 9.4);
        p.setBrush(Qt::NoBrush);
        p.drawPath(up);
    } else {                  // 平铺
        p.drawRoundedRect(QRectF(1.2, 1.2, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(QRectF(9.8, 1.2, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(QRectF(1.2, 9.8, 7, 7), 1.8, 1.8);
        p.drawRoundedRect(QRectF(9.8, 9.8, 7, 7), 1.8, 1.8);
    }
    p.end();
    return QIcon(pm);
}

QString humanSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    const double kb = bytes / 1024.0;
    if (kb < 1024)
        return QStringLiteral("%1 KB").arg(kb, 0, 'f', 0);
    const double mb = kb / 1024.0;
    if (mb < 1024)
        return QStringLiteral("%1 MB").arg(mb, mb < 10 ? 1 : 0, 'f', mb < 10 ? 1 : 0);
    return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
}

QString humanTime(const QDateTime &dt)
{
    if (!dt.isValid())
        return QString();
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QPixmap placeholderThumb(const QString &suffix, bool isVideo, int side, qreal dpr)
{
    const int px = qMax(16, int(side * dpr));
    QPixmap pm(px, px);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(0x15, 0x1A, 0x24));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(0, 0, side, side), 8, 8);
    QFont f;
    f.setPixelSize(qMax(10, side / 6));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(0x5C, 0x66, 0x78));
    p.drawText(QRectF(0, 0, side, side), Qt::AlignCenter, suffix.toUpper());
    p.end();
    return pm;
}

// 极简类型角标：视频一个播放三角，照片一个相框+山，纯描边、带一点暗影保证亮图上可见
void drawTypeGlyph(QPainter *p, const QRectF &thumb, bool isVideo)
{
    const qreal s = qBound<qreal>(13, thumb.width() * 0.075, 17);
    const QRectF box(thumb.right() - s - 9, thumb.bottom() - s - 9, s, s);

    auto paintGlyph = [&](const QColor &color, qreal dx, qreal dy) {
        QPen pen(color, 1.3);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);

        const QRectF r = box.translated(dx, dy);
        if (isVideo) {
            // 播放三角
            QPainterPath tri;
            tri.moveTo(r.left() + r.width() * 0.30, r.top() + r.height() * 0.16);
            tri.lineTo(r.left() + r.width() * 0.30, r.bottom() - r.height() * 0.16);
            tri.lineTo(r.right() - r.width() * 0.14, r.center().y());
            tri.closeSubpath();
            p->setBrush(color);
            p->setPen(Qt::NoPen);
            p->drawPath(tri);
        } else {
            // 相框 + 太阳 + 山
            p->drawRoundedRect(r.adjusted(0.6, 1.6, -0.6, -1.6), 2.2, 2.2);
            p->drawEllipse(QPointF(r.left() + r.width() * 0.33, r.top() + r.height() * 0.34),
                           1.15, 1.15);
            QPainterPath hill;
            hill.moveTo(r.left() + 1.4, r.bottom() - 2.2);
            hill.lineTo(r.left() + r.width() * 0.42, r.top() + r.height() * 0.46);
            hill.lineTo(r.left() + r.width() * 0.66, r.bottom() - 2.2);
            p->drawPath(hill);
        }
    };

    paintGlyph(QColor(0, 0, 0, 110), 0.8, 0.8);
    paintGlyph(QColor(255, 255, 255, 235), 0, 0);
}
} // namespace

// ============================================================
class AlbumModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        IsDirRole,
        IsVideoRole,
        SizeRole,
        TimeRole,
        ThumbRole,
        AspectRole,
        AppearRole,
    };

    struct Item {
        QString path;
        QString name;
        QString suffix;
        bool isDir = false;
        bool isVideo = false;
        qint64 size = 0;
        QDateTime time;
        QImage thumb;
        qreal aspect = 16.0 / 9.0;   // 宽/高：缩略图出来后按真实比例更新
        bool thumbAsked = false;
        bool failed = false;
    };

    explicit AlbumModel(QObject *parent = nullptr) : QAbstractListModel(parent)
    {
        m_animTimer = new QTimer(this);
        m_animTimer->setInterval(16);
        connect(m_animTimer, &QTimer::timeout, this, [this]() {
            const qint64 now = m_animClock.elapsed();
            bool active = false;
            for (auto it = m_appear.begin(); it != m_appear.end(); ++it) {
                const int row = it.key();
                const qint64 start = m_startAt.value(row, 0);
                if (now < start) {
                    active = true;
                    continue;
                }
                const qreal p = qBound<qreal>(0.0, (now - start) / 260.0, 1.0);
                if (p > it.value()) {
                    it.value() = p;
                    if (row < m_visible)
                        emit dataChanged(index(row), index(row), {AppearRole});
                }
                if (p < 1.0)
                    active = true;
            }
            if (!active)
                m_animTimer->stop();
        });

        connect(&ThumbnailLoader::instance(), &ThumbnailLoader::ready, this,
                [this](const QString &path, const QImage &image) {
                    const int row = m_rowByPath.value(path, -1);
                    if (row < 0 || row >= int(m_all.size()))
                        return;
                    m_all[row].thumb = image;
                    if (image.height() > 0)
                        m_all[row].aspect = qreal(image.width()) / qreal(image.height());
                    if (image.height() > 0)
                        m_all[row].aspect = qreal(image.width()) / qreal(image.height());
                    if (row < m_visible)
                        emit dataChanged(index(row), index(row), {ThumbRole});
                }, Qt::QueuedConnection);
    }

    void setSource(const QString &dir)
    {
        beginResetModel();
        m_all.clear();
        m_rowByPath.clear();
        m_appear.clear();
        m_startAt.clear();
        m_visible = 0;

        QDir d(dir);
        QFileInfoList entries = d.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDir::DirsFirst);

        for (const QFileInfo &info : entries) {
            Item item;
            item.path = info.absoluteFilePath();
            item.name = info.fileName();
            item.suffix = info.suffix().toLower();
            item.isDir = info.isDir();
            item.time = info.lastModified();
            if (!item.isDir) {
                const bool image = kImageExts.contains(item.suffix);
                const bool video = kVideoExts.contains(item.suffix);
                if (!image && !video)
                    continue;
                item.isVideo = video;
                item.size = info.size();
            }
            m_rowByPath.insert(item.path, int(m_all.size()));
            m_all.append(item);
        }
        m_visible = qMin(int(m_all.size()), kBatch);
        endResetModel();
    }

    // 过滤 / 排序在代理模型里做，这里只提供全量
    int totalCount() const { return int(m_all.size()); }
    const QList<Item> &items() const { return m_all; }

    QString pathAt(int row) const
    {
        return (row >= 0 && row < int(m_all.size())) ? m_all.at(row).path : QString();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_visible;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= m_visible)
            return {};
        const Item &item = m_all.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case PathRole:
            return item.path;
        case Qt::UserRole: // 名称
            return item.name;
        case IsDirRole:
            return item.isDir;
        case IsVideoRole:
            return item.isVideo;
        case SizeRole:
            return item.size;
        case TimeRole:
            return item.time;
        case ThumbRole:
            ensureThumb(index.row());
            return item.thumb;
        case AspectRole:
            ensureThumb(index.row());
            return item.aspect;
        case AppearRole:
            return appearProgress(index.row());
        default:
            return {};
        }
    }

    // 错峰入场 + 滚动渐显：某一行第一次被画出来时开始淡入，同屏内按行错开
    qreal appearProgress(int row) const
    {
        if (!PerfProfile::instance().albumAnimations() || PerfMonitor::instance().wallDegraded())
            return 1.0;
        AlbumModel *self = const_cast<AlbumModel *>(this);
        if (!self->m_appear.contains(row)) {
            if (!self->m_animClock.isValid())
                self->m_animClock.start();
            self->m_appear.insert(row, 0.0);
            self->m_startAt.insert(row, self->m_animClock.elapsed() + (row % 12) * 28);
            if (!self->m_animTimer->isActive())
                self->m_animTimer->start();
        }
        return self->m_appear.value(row);
    }

    void ensureThumb(int row) const
    {
        if (row < 0 || row >= int(m_all.size()))
            return;
        Item &item = const_cast<Item &>(m_all[row]);
        if (item.isDir || !item.thumb.isNull())
            return;
        // 缓存命中就直接同步读，避免异步信号错过导致一直显示占位图
        const QString cache = ThumbnailLoader::cacheFileFor(item.path);
        qInfo() << "[album] ensure" << item.name << QFileInfo::exists(cache);
        if (QFileInfo::exists(cache)) {
            QImage image(cache);
            if (!image.isNull()) {
                item.thumb = image;
                if (image.height() > 0)
                    item.aspect = qreal(image.width()) / qreal(image.height());
                return;
            }
        }
        if (item.thumbAsked)
            return;
        item.thumbAsked = true;
        ThumbnailLoader::instance().request(item.path);
    }

    bool canFetchMore(const QModelIndex &parent) const override
    {
        return !parent.isValid() && m_visible < int(m_all.size());
    }

    void fetchMore(const QModelIndex &parent) override
    {
        if (parent.isValid())
            return;
        const int from = m_visible;
        const int to = qMin(int(m_all.size()), from + kBatch);
        if (to <= from)
            return;
        beginInsertRows(QModelIndex(), from, to - 1);
        m_visible = to;
        endInsertRows();
    }

    static constexpr int kBatch = 100;

private:
    mutable QHash<int, qreal> m_appear;
    mutable QHash<int, qint64> m_startAt;
    mutable QElapsedTimer m_animClock;
    QTimer *m_animTimer = nullptr;
    QList<Item> m_all;
    QHash<QString, int> m_rowByPath;
    int m_visible = 0;
};

// ============================================================
class AlbumFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    void setMode(int mode) { m_mode = mode; invalidate(); }

    void setSearch(const QString &text)
    {
        m_search = text.trimmed();
        invalidateFilter();
    }

    // 0 时间↓ 1 时间↑ 2 大小↓ 3 大小↑ 4 名称 A-Z 5 名称 Z-A
    void setSortKey(int key)
    {
        m_sortKey = key;
        sort(0, (key == 1 || key == 3 || key == 5) ? Qt::AscendingOrder : Qt::DescendingOrder);
        invalidate();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &parent) const override
    {
        const QModelIndex idx = sourceModel()->index(sourceRow, 0, parent);
        if (!idx.isValid())
            return false;
        // 目录始终显示，方便继续往下翻
        if (idx.data(AlbumModel::IsDirRole).toBool())
            return true;
        if (m_search.isEmpty())
            return true;
        const QString name = idx.data(Qt::UserRole).toString();
        return name.contains(m_search, Qt::CaseInsensitive);
    }

    bool lessThan(const QModelIndex &a, const QModelIndex &b) const override
    {
        const bool aDir = a.data(AlbumModel::IsDirRole).toBool();
        const bool bDir = b.data(AlbumModel::IsDirRole).toBool();
        if (aDir != bDir)
            return aDir;   // 目录永远在前
        switch (m_sortKey) {
        case 2:
        case 3:
            return a.data(AlbumModel::SizeRole).toLongLong() < b.data(AlbumModel::SizeRole).toLongLong();
        case 4:
        case 5:
            return QString::compare(a.data(Qt::UserRole).toString(),
                                    b.data(Qt::UserRole).toString(),
                                    Qt::CaseInsensitive) < 0;
        default:
            return a.data(AlbumModel::TimeRole).toDateTime() < b.data(AlbumModel::TimeRole).toDateTime();
        }
    }

private:
    int m_mode = 0;
    int m_sortKey = 0;
    QString m_search;
};

// ============================================================
class AlbumDelegate : public QStyledItemDelegate
{
public:
    explicit AlbumDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void setMode(int mode)
    {
        m_mode = mode;
        if (m_mode == AlbumPage::GridMode) {
            m_cell = QSize(232, 292);
        } else if (m_mode == AlbumPage::ListMode) {
            m_cell = QSize(0, 60);
        } else {
            m_cell = QSize(176, 176);
        }
    }
    int mode() const { return m_mode; }

    // MediaWallView 直接调用：把 QStyleOptionViewItem 拼好再走原来的 paint()
    void paintItem(QPainter *p, const QRect &rect, const QModelIndex &idx, bool selected, bool hover) const
    {
        QStyleOptionViewItem opt;
        opt.rect = rect;
        opt.font = m_font;
        opt.fontMetrics = QFontMetrics(m_font);
        opt.state = QStyle::State_None;
        if (selected)
            opt.state |= QStyle::State_Selected;
        if (hover)
            opt.state |= QStyle::State_MouseOver;
        paint(p, opt, idx);
    }
    void setPaintFont(const QFont &font) { m_font = font; }
    void setCardWidth(int w) { if (w > 80) m_cardW = w; }
    int cardWidth() const { return m_cardW; }
    QSize cellSize() const { return m_cell; }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &idx) const override
    {
        Q_UNUSED(idx);
        if (m_mode == AlbumPage::ListMode)
            return QSize(0, 60);
        // 统一格子：方正、等宽等高，缩略图在里面居中裁切填满
        if (m_mode == AlbumPage::GridMode)
            return QSize(232, 292);
        return QSize(176, 176);
    }

    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);

        const bool isDir = idx.data(AlbumModel::IsDirRole).toBool();
        const bool isVideo = idx.data(AlbumModel::IsVideoRole).toBool();
        const QString name = idx.data(Qt::UserRole).toString();
        const QString path = idx.data(AlbumModel::PathRole).toString();
        const qint64 size = idx.data(AlbumModel::SizeRole).toLongLong();
        const QDateTime time = idx.data(AlbumModel::TimeRole).toDateTime();
        const QImage image = idx.data(AlbumModel::ThumbRole).value<QImage>();
        const bool selected = opt.state & QStyle::State_Selected;
        const bool hover = opt.state & QStyle::State_MouseOver;

        // 错峰入场 / 滚动渐显：淡入 + 轻微上浮（某一行第一次被画出来时才开始）
        const qreal appear = idx.data(AlbumModel::AppearRole).toDouble();
        p->setOpacity(qBound<qreal>(0.0, appear, 1.0));
        if (appear < 1.0)
            p->translate(0, (1.0 - appear) * 14.0);

        if (m_mode == AlbumPage::GridMode) {
            const QRectF card = QRectF(opt.rect).adjusted(6, 6, -6, -6);

            // 毛玻璃卡片：柔和投影 + 竖向渐变 + 顶部内高光 + 渐变描边
            QPainterPath cardPath;
            cardPath.addRoundedRect(card, 12, 12);
            p->setPen(Qt::NoPen);
            for (int i = 3; i >= 1; --i) {
                p->setBrush(QColor(0, 0, 0, 8 + i * 4));
                p->drawRoundedRect(card.adjusted(-i, -i + 2, i, i + 2), 12 + i, 12 + i);
            }
            QLinearGradient body(card.topLeft(), card.bottomLeft());
            body.setColorAt(0.0, QColor(0x24, 0x26, 0x2B));
            body.setColorAt(0.5, QColor(0x1B, 0x1C, 0x20));
            body.setColorAt(1.0, QColor(0x15, 0x16, 0x19));
            p->setBrush(body);
            p->drawPath(cardPath);

            p->save();
            p->setClipPath(cardPath);
            QLinearGradient cardTop(card.topLeft(), QPointF(card.left(), card.top() + 22));
            cardTop.setColorAt(0.0, QColor(255, 255, 255, 30));
            cardTop.setColorAt(1.0, QColor(255, 255, 255, 0));
            p->fillRect(card, cardTop);
            p->restore();

            QLinearGradient edge(card.topLeft(), card.bottomLeft());
            edge.setColorAt(0.0, QColor(255, 255, 255, 36));
            edge.setColorAt(1.0, QColor(255, 255, 255, 10));
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(QBrush(edge), 1.0));
            p->drawPath(cardPath);

            if (selected || hover) {
                p->setBrush(Qt::NoBrush);
                p->setPen(QPen(QColor(0x4C, 0x8D, 0xFF), 1.4));
                p->drawRoundedRect(card.adjusted(0.7, 0.7, -0.7, -0.7), 12, 12);
            }
            const qreal side = card.width() - 20;
            const QRectF thumb(card.left() + 10, card.top() + 10, side, side);
            QPainterPath clip;
            clip.addRoundedRect(thumb, 10, 10);
            p->setClipPath(clip);
            p->fillRect(thumb, QColor(0x12, 0x16, 0x1F));
            if (!image.isNull()) {
                drawCover(p, thumb, image);
            } else if (isDir) {
                p->fillRect(thumb, QColor(0x1A, 0x20, 0x2B));
                QFont f;
                f.setPixelSize(14);
                p->setFont(f);
                p->setPen(QColor(0x60, 0xA5, 0xFA));
                p->drawText(thumb, Qt::AlignCenter, QStringLiteral("文件夹"));
            } else {
                p->drawPixmap(thumb.toRect(), placeholderThumb(QFileInfo(name).suffix(), isVideo, int(thumb.width()), p->device()->devicePixelRatioF()));
            }
            p->setClipping(false);
            if (!isDir)
                drawTypeGlyph(p, thumb, isVideo);

            QFont f = opt.font;
            f.setPixelSize(14);
            p->setFont(f);
            p->setPen(QColor(0xFF, 0xFF, 0xFF));
            const QRectF nameRect(card.left() + 12, thumb.bottom() + 10, card.width() - 24, 20);
            p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                        opt.fontMetrics.elidedText(name, Qt::ElideMiddle, int(nameRect.width())));
            f.setPixelSize(12);
            p->setFont(f);
            p->setPen(QColor(0x9A, 0xA0, 0xA8));
            const QString meta = isDir ? QStringLiteral("文件夹")
                                       : QStringLiteral("%1 · %2").arg(humanSize(size), humanTime(time));
            p->drawText(QRectF(card.left() + 12, nameRect.bottom() + 2, card.width() - 24, 18),
                        Qt::AlignLeft | Qt::AlignVCenter,
                        opt.fontMetrics.elidedText(meta, Qt::ElideRight, int(card.width() - 24)));
        } else if (m_mode == AlbumPage::ListMode) {
            const QRectF row = QRectF(opt.rect).adjusted(4, 3, -4, -3);
            if (selected) {
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0x17, 0x18, 0x1B));
                p->drawRoundedRect(row, 8, 8);
                p->setBrush(QColor(0x4C, 0x8D, 0xFF));
                p->drawRoundedRect(QRectF(row.left(), row.top() + 4, 3, row.height() - 8), 1.5, 1.5);
            } else if (hover) {
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0x17, 0x18, 0x1B));
                p->drawRoundedRect(row, 8, 8);
            }
            const QRectF thumb(row.left() + 12, row.center().y() - 22, 56, 56);
            QPainterPath clip;
            clip.addRoundedRect(thumb, 6, 6);
            p->setClipPath(clip);
            if (!image.isNull())
                p->drawImage(thumb, image, QRectF(image.rect()));
            else if (isDir)
                p->fillRect(thumb, QColor(0x1A, 0x20, 0x2B));
            else
                p->drawPixmap(thumb.toRect(),
                              placeholderThumb(QStringLiteral("?"), isVideo, 48, p->device()->devicePixelRatioF()));
            p->setClipping(false);
            if (!isDir)
                drawTypeGlyph(p, thumb, isVideo);

            QFont f = opt.font;
            f.setPixelSize(14);
            p->setFont(f);
            p->setPen(QColor(0xFF, 0xFF, 0xFF));
            const qreal textLeft = thumb.right() + 14;
            const QRectF nameRect(textLeft, row.top() + 10, row.width() - textLeft - 160, 20);
            p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                        opt.fontMetrics.elidedText(name, Qt::ElideMiddle, int(nameRect.width())));
            f.setPixelSize(12);
            p->setFont(f);
            p->setPen(QColor(0x9A, 0xA0, 0xA8));
            const QString meta = isDir ? QStringLiteral("文件夹")
                                       : QStringLiteral("%1  ·  %2").arg(humanSize(size), humanTime(time));
            p->drawText(QRectF(textLeft, nameRect.bottom() + 2, row.width() - textLeft - 20, 18),
                        Qt::AlignLeft | Qt::AlignVCenter, meta);
        } else {
            // 平铺：只有缩略图（文件夹没有缩略图时给个文字提示）
            const QRectF cell = QRectF(opt.rect).adjusted(3, 3, -3, -3);
            QPainterPath clip;
            clip.addRoundedRect(cell, 10, 10);
            p->setClipPath(clip);
            if (!image.isNull()) {
                p->drawImage(cell, image, QRectF(image.rect()));
            } else if (isDir) {
                p->fillRect(cell, QColor(0x1A, 0x20, 0x2B));
                QFont f = opt.font;
                f.setPixelSize(14);
                p->setFont(f);
                p->setPen(QColor(0x60, 0xA5, 0xFA));
                p->drawText(cell, Qt::AlignCenter, QStringLiteral("文件夹"));
            } else {
                p->fillRect(cell, QColor(0x17, 0x18, 0x1B));
                QFont f = opt.font;
                f.setPixelSize(12);
                p->setFont(f);
                p->setPen(QColor(0x5C, 0x66, 0x78));
                p->drawText(cell, Qt::AlignCenter,
                            QFileInfo(name).suffix().toUpper());
            }
            p->setClipping(false);
            if (!isDir)
                drawTypeGlyph(p, cell, isVideo);
            if (selected) {
                p->setBrush(Qt::NoBrush);
                p->setPen(QPen(QColor(0x4C, 0x8D, 0xFF), 2));
                p->drawRoundedRect(cell.adjusted(1, 1, -1, -1), 10, 10);
            }
        }
        p->restore();
    }

private:
    int m_mode = AlbumPage::GridMode;
    int m_cardW = 232;
    QSize m_cell{196, 232};
    QFont m_font;
};

// ============================================================
AlbumPage::AlbumPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 20, 26, 18);
    root->setSpacing(14);

    auto *header = new QHBoxLayout();
    header->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("素材浏览"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    header->addWidget(title);

    m_breadcrumb = new QLabel(this);
    m_breadcrumb->setObjectName(QStringLiteral("hintLabel"));
    m_breadcrumb->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->addWidget(m_breadcrumb, 1);

    m_hint = new QLabel(QStringLiteral("预览画质已压缩，不代表原片画质"), this);
    m_hint->setStyleSheet(QStringLiteral("color:#9C9C9F;font-size:12px;"));
    header->addWidget(m_hint, 0, Qt::AlignRight);
    root->addLayout(header);
    {
        auto *glass = new GlassPanel(this);
        glass->setRadius(14);
        glass->setSheenStrength(0.07);
        auto *tb = buildToolbar();
        tb->setContentsMargins(10, 8, 10, 8);
        glass->setLayout(tb);
        root->addWidget(glass);
    }


    m_model = new AlbumModel(this);
    auto *filter = new AlbumFilterModel();
    filter->setSourceModel(m_model);
    filter->setDynamicSortFilter(true);

    m_delegate = new AlbumDelegate(this);
    m_delegate->setPaintFont(font());
    auto *delegate = m_delegate;

    m_wall = new MediaWallView(this);
    m_wall->setObjectName(QStringLiteral("albumView"));
    m_wall->setModel(filter);
    m_wall->setItemPainter([delegate](QPainter *p, const QRect &rect, const QModelIndex &idx,
                                      bool selected, bool hover) {
        delegate->paintItem(p, rect, idx, selected, hover);
    });

    m_wall->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_wall->setMinimumWidth(200);
    root->addWidget(m_wall, 1);   // QAbstractScrollArea 自带滚动条

    m_empty = new QLabel(QStringLiteral("这个文件夹里还没有照片或视频"), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setStyleSheet(QStringLiteral("color:#9C9C9F;font-size:14px;"));
    m_empty->hide();
    root->addWidget(m_empty);

    connect(m_wall, &MediaWallView::activated, this, [this](const QModelIndex &idx) {
        if (idx.data(AlbumModel::IsDirRole).toBool())
            setRootPath(idx.data(AlbumModel::PathRole).toString());
        else
            onActivated(idx);
    });


    connect(m_search, &QLineEdit::textChanged, this, &AlbumPage::onSearchChanged);
    connect(m_sort, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { onSortChanged(index); });
    connect(m_gridBtn, &QToolButton::clicked, this, [this]() { onViewModeChanged(GridMode); });
    connect(m_listBtn, &QToolButton::clicked, this, [this]() { onViewModeChanged(ListMode); });
    connect(m_tileBtn, &QToolButton::clicked, this, [this]() { onViewModeChanged(TileMode); });

    onViewModeChanged(GridMode);
    followTargetFolder();
}

QHBoxLayout *AlbumPage::buildToolbar()
{
    auto *row = new QHBoxLayout();
    row->setSpacing(8);

    auto makeViewBtn = [this](int mode, const QString &tip) {
        auto *btn = new QToolButton(this);
        btn->setObjectName(QStringLiteral("albumViewBtn"));
        btn->setIcon(viewModeIcon(mode));
        btn->setIconSize(QSize(18, 18));
        btn->setToolTip(tip);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(36, 32);
        return btn;
    };
    m_gridBtn = makeViewBtn(0, QStringLiteral("网格视图"));
    m_listBtn = makeViewBtn(1, QStringLiteral("列表视图"));
    m_tileBtn = makeViewBtn(2, QStringLiteral("平铺视图"));
    row->addWidget(m_gridBtn);
    row->addWidget(m_listBtn);
    row->addWidget(m_tileBtn);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("albumSearch"));
    m_search->setPlaceholderText(QStringLiteral("按文件名搜索"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(240);
    row->addWidget(m_search);

    m_sort = new QComboBox(this);
    m_sort->setObjectName(QStringLiteral("albumSort"));
    m_sort->addItems({QStringLiteral("按时间 ↓"), QStringLiteral("按时间 ↑"),
                      QStringLiteral("按大小 ↓"), QStringLiteral("按大小 ↑"),
                      QStringLiteral("按名称 A-Z"), QStringLiteral("按名称 Z-A")});
    m_sort->setFixedWidth(140);
    row->addWidget(m_sort);

    auto *refreshBtn = new QToolButton(this);
    refreshBtn->setObjectName(QStringLiteral("albumViewBtn"));
    refreshBtn->setIcon(viewModeIcon(3));
    refreshBtn->setToolTip(QStringLiteral("刷新"));
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setFixedSize(36, 32);
    connect(refreshBtn, &QToolButton::clicked, this, &AlbumPage::refresh);
    row->addWidget(refreshBtn);

    auto *upBtn = new QToolButton(this);
    upBtn->setObjectName(QStringLiteral("albumViewBtn"));
    upBtn->setIcon(viewModeIcon(4));
    upBtn->setToolTip(QStringLiteral("上一级"));
    upBtn->setCursor(Qt::PointingHandCursor);
    upBtn->setFixedSize(36, 32);
    connect(upBtn, &QToolButton::clicked, this, &AlbumPage::goUp);
    row->addWidget(upBtn);

    row->addStretch();
    return row;
}

// 把墙的宽度钉死在滚动区可视宽度上：QScrollArea 有时给不出正确宽度，
// 导致卡片排到可视区外（右侧被裁）
void AlbumPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 每次切到这一页都跟着目标文件夹走一遍（备份完目标目录会变）
    followTargetFolder();
}

void AlbumPage::followTargetFolder()
{
    const QString target = SettingsManager::instance()->targetPath();
    setRootPath(target);
}

void AlbumPage::setRootPath(const QString &path)
{
    if (path.isEmpty())
        return;
    QDir dir(path);
    if (!dir.exists())
        return;
    m_rootPath = dir.absolutePath();
    updateBreadcrumb();

    auto *model = qobject_cast<AlbumModel *>(
        static_cast<AlbumFilterModel *>(m_wall->model())->sourceModel());
    if (model)
        model->setSource(m_rootPath);
    updateEmptyState();
}

void AlbumPage::updateBreadcrumb()
{
    if (m_rootPath.isEmpty()) {
        m_breadcrumb->setText(QStringLiteral("未设置目标文件夹"));
        return;
    }
    const QString native = QDir::toNativeSeparators(m_rootPath);
    m_breadcrumb->setText(native);
    m_breadcrumb->setToolTip(native);
}

void AlbumPage::updateEmptyState()
{
    auto *filter = qobject_cast<AlbumFilterModel *>(m_wall->model());
    const bool empty = !filter || filter->rowCount() == 0;
    m_empty->setVisible(empty);
    m_wall->setVisible(!empty);

}

void AlbumPage::refresh()
{
    if (m_rootPath.isEmpty()) {
        followTargetFolder();
        return;
    }
    setRootPath(m_rootPath);
    Toast::showToast(QStringLiteral("已刷新"), QStringLiteral("重新读取了当前文件夹"),
                     Toast::Success, 1600);
}

void AlbumPage::goUp()
{
    QDir dir(m_rootPath);
    if (!dir.cdUp())
        return;
    setRootPath(dir.absolutePath());
}

void AlbumPage::onViewModeChanged(int mode)
{
    m_mode = mode;
    m_gridBtn->setChecked(mode == GridMode);
    m_listBtn->setChecked(mode == ListMode);
    m_tileBtn->setChecked(mode == TileMode);

    if (m_delegate)
        m_delegate->setMode(mode);

    if (m_wall) {
        m_wall->setMode(mode == ListMode ? MediaWallView::ListMode
                                         : (mode == TileMode ? MediaWallView::TileMode
                                                             : MediaWallView::GridMode));
        m_wall->relayout();
    }
}
void AlbumPage::onSearchChanged(const QString &text)
{
    m_filter = text;
    auto *filter = qobject_cast<AlbumFilterModel *>(m_wall->model());
    if (filter)
        filter->setSearch(text);
    updateEmptyState();
}

void AlbumPage::onSortChanged(int index)
{
    auto *filter = qobject_cast<AlbumFilterModel *>(m_wall->model());
    if (filter)
        filter->setSortKey(index);
}

void AlbumPage::onActivated(const QModelIndex &index)
{
    const QString path = index.data(AlbumModel::PathRole).toString();
    if (!path.isEmpty())
        openPlayer(path);
}

void AlbumPage::openPlayer(const QString &path)
{
    // 收集当前文件夹里所有视频，做成播放列表
    QStringList videos;
    int current = 0;
    if (m_wall && m_wall->model()) {
        QAbstractItemModel *m = m_wall->model();
        for (int i = 0; i < m->rowCount(); ++i) {
            const QModelIndex idx = m->index(i, 0);
            if (!idx.data(AlbumModel::IsVideoRole).toBool())
                continue;
            const QString p = idx.data(AlbumModel::PathRole).toString();
            if (p.isEmpty())
                continue;
            if (p == path)
                current = videos.size();
            videos << p;
        }
    }

    auto *player = new MediaPlayerWindow(path, this);
    if (videos.size() > 1)
        player->setPlaylist(videos, current);
    player->setAttribute(Qt::WA_DeleteOnClose, true);
    player->show();
    player->raise();
    player->activateWindow();
}

#include "album_page.moc"
