#ifndef MEDIAWALL_H
#define MEDIAWALL_H

#include <QAbstractItemModel>
#include <QAbstractScrollArea>
#include <QModelIndex>
#include <QVector>

#include <functional>

/**
 * 素材墙。
 *
 * 直接继承 QAbstractScrollArea：viewport 的宽度就是可用宽度，
 * 卡片按列均分整行，窗口怎么拖都不会留白、也不会把最后一列裁掉。
 *
 *   cols  = 可用宽度 / (最小卡宽 + 间距)
 *   cardW = (可用宽度 - 间距 × (cols-1)) / cols
 */
class MediaWallView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    enum Mode { GridMode, ListMode, TileMode };

    explicit MediaWallView(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const { return m_model; }

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    using ItemPainter = std::function<void(QPainter *, const QRect &, const QModelIndex &, bool, bool)>;
    void setItemPainter(ItemPainter painter) { m_painter = std::move(painter); }

    QModelIndex indexAt(const QPoint &pos) const;
    QModelIndex currentIndex() const { return m_current; }
    void setCurrentIndex(const QModelIndex &index);
    void clearCurrent();

    void relayout();
    int contentHeight() const { return m_contentHeight; }

signals:
    void activated(const QModelIndex &index);
    void currentChanged(const QModelIndex &index);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QRect rectForRow(int row) const;

    QAbstractItemModel *m_model = nullptr;
    Mode m_mode = GridMode;
    ItemPainter m_painter;
    QVector<QRect> m_rects;
    int m_contentHeight = 0;
    int m_layoutWidth = -1;
    int m_hoverRow = -1;
    int m_scrollOffset = 0;
    QModelIndex m_current;
};

#endif // MEDIAWALL_H
