#ifndef FLOWCANVAS_H
#define FLOWCANVAS_H

#include <QLabel>
#include <QPair>
#include <QPointer>
#include <QWidget>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QList>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/flowpath.h"

class QLabel;
class QTimer;
class QPainter;
class QPropertyAnimation;
class QMouseEvent;
class QResizeEvent;
class QStyleOptionGraphicsItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneDragDropEvent;

class FlowCanvas;
class FlowScene;

namespace FlowMime {
inline constexpr const char *Format = "application/x-cjibackup-flow";
}

/** 分类项定义（面板上的可拖拽项） */
struct FlowPaletteItem
{
    QString key;    // year / month / day / event / seq / ext / custom
    QString name;   // 显示名
    QString hint;   // 说明文字
};

QVector<FlowPaletteItem> flowPaletteItems();
const FlowPaletteItem *flowPaletteItem(const QString &key);

/** 画布上的分类节点（单个分类项，或几个分类项合并成的一个节点） */
class FlowNodeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    FlowNodeItem(const QString &key, const QString &text, QGraphicsItem *parent = nullptr);
    FlowNodeItem(const QVector<FlowPath::Item> &members, const QString &separator,
                 QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    bool isDragging() const { return m_dragging; }

    // 合并节点
    bool isMerged() const { return m_merged; }
    QVector<FlowPath::Item> members() const;
    QString separator() const { return m_sep; }
    void setSeparator(const QString &sep);
    void setMemberText(int index, const QString &text);
    QStringList sampleParts() const;   // 合并节点预览用（2026 / 10 / 31）

    QString key() const { return m_key; }
    QString text() const { return m_text; }
    void setText(const QString &text);

    int level() const { return m_level; }
    void setLevel(int level);

    void setChainActive(bool active);
    bool chainActive() const { return m_active; }

    QPointF leftPort() const;
    QPointF rightPort() const;

    QString title() const;
    QString subtitle() const;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    void initItem();

    QString m_key;
    QString m_text;
    QVector<FlowPath::Item> m_members;   // 合并节点的成员；空 = 单个节点
    QString m_sep = QStringLiteral("-");
    bool m_merged = false;
    int m_level = 0;
    bool m_active = true;
    bool m_hovered = false;
    bool m_dragging = false;
    QPointF m_pressScenePos;
    QPointF m_pressItemPos;
};

/** 流程图场景：节点布局、连线、拖放 */
class FlowScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit FlowScene(FlowCanvas *canvas, QObject *parent = nullptr);

    QStringList config() const;
    void setConfig(const QStringList &config);
    void clearAll();
    void setCanvasSize(const QSize &size);
    int nodeCount() const { return m_nodes.size(); }
    bool containsKey(const QString &key) const;

    // 由 FlowNodeItem 在拖动结束时调用
    void finishNodeDrag(FlowNodeItem *node);

    // 双击合并节点 / 右键菜单：改分隔符
    void editMergedSeparator(FlowNodeItem *node);
    // 拆开合并节点（右键菜单 / 测试用）
    void unmergeNode(FlowNodeItem *node);

    // 从分类项面板插入一个节点；level < 0 表示追加到末尾
    void insertFromPalette(const QString &key, int level);
    // 从分类项面板直接拖到某个节点身上：合并进这个节点
    void addPaletteItemToNode(const QString &key, FlowNodeItem *target, bool before);

signals:
    void changed();
    void statusMessage(const QString &message);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragLeaveEvent(QGraphicsSceneDragDropEvent *event) override;
    void dropEvent(QGraphicsSceneDragDropEvent *event) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    FlowNodeItem *nodeAt(const QPointF &scenePos) const;
    FlowNodeItem *mergeTargetAt(FlowNodeItem *node) const;
    void updateDropIndicator(const QPointF &scenePos);
    int levelForX(qreal x) const;

    void insertNode(const QString &key, const QString &text, int level);
    void appendMergedNode(const QVector<FlowPath::Item> &members, const QString &separator);
    void mergeNodes(FlowNodeItem *dragged, FlowNodeItem *target);
    void removeNodeWithConfirm(FlowNodeItem *node);
    void restoreLinks(const QVector<QPair<int, int>> &links);
    void relinkByOrder();
    void removeNode(FlowNodeItem *node);
    void reindex();
    void relayout(bool animate);
    void rebuildChain();
    void link(FlowNodeItem *from, FlowNodeItem *to);
    void unlinkRight(FlowNodeItem *from);
    void autoLinkAll();
    void notifyChanged();

    void showNodeMenu(FlowNodeItem *node, const QPoint &screenPos);
    void showCanvasMenu(const QPoint &screenPos);
    void paintArrow(QPainter *painter, const QPointF &from, const QPointF &to,
                    bool active, bool dashed);

    FlowCanvas *m_canvas = nullptr;
    QVector<FlowNodeItem *> m_nodes;
    QHash<FlowNodeItem *, FlowNodeItem *> m_next;
    QHash<FlowNodeItem *, FlowNodeItem *> m_prev;
    QList<QPointer<QPropertyAnimation>> m_anims;

    QSize m_canvasSize{600, 170};
    bool m_loading = false;
    bool m_layingOut = false;
    bool m_resizePending = false;

    FlowNodeItem *m_connectFrom = nullptr;
    QPointF m_connectPoint;
    bool m_connectDragging = false;
    bool m_suppressContextMenu = false;

    int m_dropLevel = -1;
    FlowNodeItem *m_dropTarget = nullptr;   // 拖分类项时悬停的节点（松手就合并进去）
    qreal m_pitch = 138.0;
};

/** 支持尺寸通知的画布视图 */
class FlowView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit FlowView(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void applyCanvasSize();

    bool m_sizeScheduled = false;
};

/** 面板上的可拖拽分类项 */
class FlowChip : public QLabel
{
    Q_OBJECT

public:
    explicit FlowChip(const FlowPaletteItem &item, QWidget *parent = nullptr);

signals:
    void addRequested(const QString &key);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    FlowPaletteItem m_item;
    QPoint m_pressPos;
    bool m_mayDrag = false;
};

/** 流程图分类组件：上方分类项面板 + 下方画布 + 目录预览 */
class FlowCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit FlowCanvas(QWidget *parent = nullptr);

    QStringList flowConfig() const;
    void setFlowConfig(const QStringList &config);
    void clearAll();
    // 恢复一份流程配置并通知外部保存（撤销用）
    void restoreConfig(const QStringList &config);

    // 全局日期分隔符变了：只刷新目录预览，不动画布
    void refreshPreview();

    // 相机型号节点 / 目录预览里显示的型号（选完源目录后传入真实型号）
    void setCameraModelHint(const QString &model);

    QSize sizeHint() const override;

    // 弹出对话框获取事件名 / 自定义文本（供场景调用）
    bool askNodeText(const QString &key, const QString &initial, QString *text);
    // 弹出分隔符选择框（合并节点用）
    bool askMergeSeparator(const QString &initial, const QStringList &sampleParts,
                           QString *separator);

signals:
    void flowChanged(const QStringList &config);

public slots:
    void addPaletteItem(const QString &key);   // 双击面板项时追加到流程末尾

private:
    void updatePreview();
    void showStatus(const QString &message);

    FlowScene *m_scene = nullptr;
    FlowView *m_view = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_statusTimer = nullptr;
};

#endif // FLOWCANVAS_H
