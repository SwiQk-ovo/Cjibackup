#include "widgets/mediawall.h"

#include <QAbstractItemModel>
#include <QElapsedTimer>

#include "core/perfmonitor.h"
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

namespace {
constexpr int kGap = 12;         // 卡片间距
constexpr int kPadSide = 0;
constexpr int kPadTop = 4;
constexpr int kLabelHeight = 58; // 网格模式：缩略图下方两行文字
constexpr int kListRowHeight = 60;
} // namespace

MediaWallView::MediaWallView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setMouseTracking(true);
    viewport()->setAutoFillBackground(false);
    viewport()->setStyleSheet(QStringLiteral("background: transparent;"));   // 否则 viewport 会刷成白底
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    setStyleSheet(QStringLiteral("QScrollBar:vertical{width:10px;background:transparent;}"
                                 "QScrollBar::handle:vertical{background:#2E2E31;border-radius:5px;min-height:40px;}"
                                 "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
        m_scrollOffset = v;
        viewport()->update();
    });
}

void MediaWallView::setModel(QAbstractItemModel *model)
{
    if (m_model == model)
        return;
    if (m_model)
        m_model->disconnect(this);
    m_model = model;
    if (m_model) {
        connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { relayout(); });
        connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]() { relayout(); });
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this]() { relayout(); });
        connect(m_model, &QAbstractItemModel::layoutChanged, this, [this]() { relayout(); });
        connect(m_model, &QAbstractItemModel::dataChanged, this, [this]() { viewport()->update(); });
    }
    relayout();
}

void MediaWallView::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    relayout();
}

void MediaWallView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    relayout();
}

void MediaWallView::showEvent(QShowEvent *event)
{
    QAbstractScrollArea::showEvent(event);
    relayout();
}

void MediaWallView::relayout()
{
    m_rects.clear();
    if (!m_model || m_model->rowCount() == 0) {
        m_contentHeight = 0;
        verticalScrollBar()->setRange(0, 0);
        viewport()->update();
        return;
    }

    const int count = m_model->rowCount();
    // viewport 宽度是滚动区自己算好的，绝不会有误差
    const int avail = qMax(240, viewport()->width() - kPadSide * 2);

    if (m_mode == ListMode) {
        m_rects.reserve(count);
        for (int i = 0; i < count; ++i)
            m_rects.append(QRect(kPadSide, kPadTop + i * kListRowHeight, avail, kListRowHeight));
        m_contentHeight = kPadTop + count * kListRowHeight + 8;
    } else {
        const int minCardW = (m_mode == TileMode ? 150 : 200);
        int cols = (avail + kGap) / (minCardW + kGap);
        cols = qBound(3, cols, 8);
        const int cardW = (avail - kGap * (cols - 1)) / cols;
        const int cardH = (m_mode == TileMode) ? cardW : cardW + kLabelHeight;

        m_rects.reserve(count);
        for (int i = 0; i < count; ++i) {
            const int row = i / cols;
            const int col = i % cols;
            m_rects.append(QRect(kPadSide + col * (cardW + kGap),
                                 kPadTop + row * (cardH + kGap), cardW, cardH));
        }
        const int rows = (count + cols - 1) / cols;
        m_contentHeight = kPadTop + rows * (cardH + kGap) + 8;
    }

    verticalScrollBar()->setRange(0, qMax(0, m_contentHeight - viewport()->height()));
    verticalScrollBar()->setPageStep(qMax(40, viewport()->height()));
    verticalScrollBar()->setSingleStep(80);
    m_layoutWidth = viewport()->width();
    viewport()->update();
}

QRect MediaWallView::rectForRow(int row) const
{
    if (row < 0 || row >= m_rects.size())
        return QRect();
    return m_rects.at(row).translated(0, -m_scrollOffset);
}

QModelIndex MediaWallView::indexAt(const QPoint &pos) const
{
    const QPoint p = pos + QPoint(0, m_scrollOffset);
    for (int i = 0; i < m_rects.size(); ++i) {
        if (m_rects.at(i).contains(p))
            return m_model ? m_model->index(i, 0) : QModelIndex();
    }
    return QModelIndex();
}

void MediaWallView::setCurrentIndex(const QModelIndex &index)
{
    if (!index.isValid() || index == m_current)
        return;
    m_current = index;
    emit currentChanged(m_current);
    viewport()->update();
}

void MediaWallView::clearCurrent()
{
    m_current = QModelIndex();
    viewport()->update();
}

void MediaWallView::paintEvent(QPaintEvent *event)
{
    QElapsedTimer frameTimer;
    frameTimer.start();

    if (m_layoutWidth != viewport()->width())
        relayout();

    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.translate(0, -m_scrollOffset);

    if (!m_model || !m_painter || m_rects.isEmpty())
        return;

    // 只画可视范围内的行：滚动时不再遍历全部项目
    const int viewTop = m_scrollOffset;
    const int viewBottom = m_scrollOffset + viewport()->height();
    auto firstVisible = [&]() {
        int lo = 0, hi = m_rects.size() - 1, ans = m_rects.size();
        while (lo <= hi) {
            const int mid = (lo + hi) / 2;
            if (m_rects.at(mid).bottom() >= viewTop) { ans = mid; hi = mid - 1; }
            else lo = mid + 1;
        }
        return ans;
    };

    const QRect clip = event->rect().translated(0, m_scrollOffset);
    for (int i = firstVisible(); i < m_rects.size(); ++i) {
        const QRect r = m_rects.at(i);
        if (r.top() > viewBottom)
            break;
        if (!r.intersects(clip))
            continue;
        const QModelIndex idx = m_model->index(i, 0);
        if (!idx.isValid())
            continue;
        m_painter(&p, r, idx, idx == m_current, i == m_hoverRow);
    }
    PerfMonitor::instance().reportWallFrame(int(frameTimer.elapsed()));

}
void MediaWallView::mouseMoveEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->pos());
    const int row = idx.isValid() ? idx.row() : -1;
    if (row != m_hoverRow) {
        m_hoverRow = row;
        viewport()->setCursor(row >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        viewport()->update();
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

void MediaWallView::leaveEvent(QEvent *event)
{
    if (m_hoverRow != -1) {
        m_hoverRow = -1;
        viewport()->update();
    }
    QAbstractScrollArea::leaveEvent(event);
}

void MediaWallView::mousePressEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->pos());
    if (idx.isValid())
        setCurrentIndex(idx);
    else
        clearCurrent();
    QAbstractScrollArea::mousePressEvent(event);
}

void MediaWallView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const QModelIndex idx = indexAt(event->pos());
    if (idx.isValid())
        emit activated(idx);
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}
