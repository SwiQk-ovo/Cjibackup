#include "widgets/squircle.h"
#include "flowcanvas.h"
#include "core/flowpath.h"
#include "core/settingsmanager.h"
#include "widgets/darkdialog.h"
#include "widgets/separatordialog.h"
#include "widgets/toast.h"

#include <QAbstractAnimation>
#include <QApplication>
#include <QDateTime>
#include <QDrag>
#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSet>
#include <QStyleOptionGraphicsItem>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kNodeW = 104;
constexpr int kNodeH = 52;
constexpr int kGap = 34;
constexpr int kPitch = kNodeW + kGap;
constexpr int kMarginX = 22;
constexpr int kMinCanvasH = 126;
constexpr int kMaxLevels = 8;

const QColor kCardBg(28, 33, 44);
const QColor kCardBgDim(22, 26, 34);
const QColor kCardBorder(42, 47, 58);
const QColor kAccent(59, 130, 246);
const QColor kAccentSoft(96, 165, 250);
const QColor kTextMain(255, 255, 255);
const QColor kTextDim(148, 155, 168);
const QColor kTextMuted(120, 128, 142);
const QColor kCanvasBg(19, 22, 31);
const QColor kLinkIdle(57, 64, 79);

QString elided(QPainter *painter, const QString &text, qreal width)
{
    const QFontMetrics fm(painter->font());
    return fm.elidedText(text, Qt::ElideRight, int(width));
}

double animFactor()
{
    double speed = SettingsManager::instance()->animationSpeed();
    if (speed <= 0.0)
        speed = 1.0;
    return speed;
}

// 相机型号节点 / 目录预览里显示的型号。
// 用户在备份页选完源目录后会被真实型号覆盖，没选之前用这个示例。
QString &cameraHintModel()
{
    static QString hint = QStringLiteral("Nikon Z 6II");
    return hint;
}

// 节点上显示的示例值（年 2026 / 月 10 / 日 31 / 事件 校运会 …）
QString sampleForItem(const FlowPath::Item &item)
{
    const QDateTime now = QDateTime::currentDateTime();
    switch (item.key) {
    case FlowPath::Key::Year:
        return now.toString("yyyy");
    case FlowPath::Key::Month:
        return now.toString("MM");
    case FlowPath::Key::Day:
        return now.toString("dd");
    case FlowPath::Key::Event:
        return item.text.isEmpty() ? QStringLiteral("未命名事件") : item.text;
    case FlowPath::Key::Custom:
        return item.text.isEmpty() ? QStringLiteral("（待填写）") : item.text;
    case FlowPath::Key::Camera:
        return cameraHintModel();
    case FlowPath::Key::Ext:
        return QStringLiteral("JPG");
    case FlowPath::Key::Seq:
        return QStringLiteral("0001");
    }
    return QString();
}

QString itemTitle(const FlowPath::Item &item)
{
    if (const FlowPaletteItem *palette = flowPaletteItem(FlowPath::keyName(item.key)))
        return palette->name;
    return FlowPath::keyName(item.key);
}

} // namespace

/* ------------------------------------------------------------------ */
/* 分类项定义                                                          */
/* ------------------------------------------------------------------ */

QVector<FlowPaletteItem> flowPaletteItems()
{
    static const QVector<FlowPaletteItem> items = {
        {QStringLiteral("year"), QStringLiteral("年"), QStringLiteral("按拍摄年份建文件夹，如 2026")},
        {QStringLiteral("month"), QStringLiteral("月"), QStringLiteral("按月份建文件夹，如 10")},
        {QStringLiteral("day"), QStringLiteral("日"), QStringLiteral("按日期建文件夹，如 31")},
        {QStringLiteral("camera"), QStringLiteral("相机型号"), QStringLiteral("按相机型号建文件夹，如 Nikon Z 6II")},
        {QStringLiteral("event"), QStringLiteral("事件"), QStringLiteral("自定义事件名，如 校运会")},
        {QStringLiteral("seq"), QStringLiteral("序号"), QStringLiteral("文件名编号，如 0001")},
        {QStringLiteral("ext"), QStringLiteral("后缀"), QStringLiteral("按文件类型建文件夹，如 JPG")},
        {QStringLiteral("custom"), QStringLiteral("自定义"), QStringLiteral("任意文件夹名")}
    };
    return items;
}

const FlowPaletteItem *flowPaletteItem(const QString &key)
{
    const QVector<FlowPaletteItem> items = flowPaletteItems();
    for (const FlowPaletteItem &item : items) {
        if (item.key == key)
            return &item;
    }
    return nullptr;
}

/* ------------------------------------------------------------------ */
/* FlowNodeItem                                                        */
/* ------------------------------------------------------------------ */

FlowNodeItem::FlowNodeItem(const QString &key, const QString &text, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_key(key)
    , m_text(text)
{
    initItem();
}

FlowNodeItem::FlowNodeItem(const QVector<FlowPath::Item> &members, const QString &separator,
                           QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_members(members)
    , m_sep(separator.isEmpty() ? QStringLiteral("-") : separator)
    , m_merged(!members.isEmpty())
{
    if (!members.isEmpty())
        m_key = FlowPath::keyName(members.first().key);
    initItem();
    setToolTip(QStringLiteral("合并节点：双击可改分隔符，右键可拆分"));
}

void FlowNodeItem::initItem()
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setZValue(1.0);
}

QRectF FlowNodeItem::boundingRect() const
{
    return QRectF(-6, -6, kNodeW + 12, kNodeH + 12);
}

QPointF FlowNodeItem::leftPort() const
{
    return pos() + QPointF(0, kNodeH / 2.0);
}

QPointF FlowNodeItem::rightPort() const
{
    return pos() + QPointF(kNodeW, kNodeH / 2.0);
}

QVector<FlowPath::Item> FlowNodeItem::members() const
{
    if (m_merged)
        return m_members;
    FlowPath::Item item;
    if (!FlowPath::keyFromName(m_key, &item.key))
        return {};
    item.text = m_text;
    return {item};
}

void FlowNodeItem::setSeparator(const QString &sep)
{
    const QString value = sep.isEmpty() ? QStringLiteral("-") : sep;
    if (m_sep == value)
        return;
    m_sep = value;
    update();
}

void FlowNodeItem::setMemberText(int index, const QString &text)
{
    if (index < 0 || index >= m_members.size())
        return;
    m_members[index].text = text;
    if (index == 0)
        m_text = text;
    update();
}

QStringList FlowNodeItem::sampleParts() const
{
    QStringList parts;
    for (const FlowPath::Item &item : members())
        parts << sampleForItem(item);
    return parts;
}

void FlowNodeItem::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    update();
}

void FlowNodeItem::setLevel(int level)
{
    m_level = level;
}

void FlowNodeItem::setChainActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    setOpacity(active ? 1.0 : 0.42);
    update();
}

QString FlowNodeItem::title() const
{
    if (m_merged) {
        QStringList names;
        for (const FlowPath::Item &item : m_members)
            names << itemTitle(item);
        return names.join(QStringLiteral("·"));
    }
    const FlowPaletteItem *item = flowPaletteItem(m_key);
    return item ? item->name : m_key;
}

QString FlowNodeItem::subtitle() const
{
    if (m_merged) {
        QStringList parts = sampleParts();
        parts.removeAll(QString());
        return parts.join(m_sep);
    }

    if (m_key == QStringLiteral("event"))
        return m_text.isEmpty() ? QStringLiteral("（待命名）") : m_text;
    if (m_key == QStringLiteral("custom"))
        return m_text.isEmpty() ? QStringLiteral("（待填写）") : m_text;
    if (m_key == QStringLiteral("camera"))
        return cameraHintModel();

    const QDateTime now = QDateTime::currentDateTime();
    if (m_key == QStringLiteral("year"))
        return now.toString("yyyy");
    if (m_key == QStringLiteral("month"))
        return now.toString("MM");
    if (m_key == QStringLiteral("day"))
        return now.toString("dd");
    if (m_key == QStringLiteral("seq"))
        return QStringLiteral("0001");
    if (m_key == QStringLiteral("ext"))
        return QStringLiteral("JPG");
    return QString();
}

void FlowNodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF body(0, 0, kNodeW, kNodeH);
    const QColor border = m_active
                              ? (m_hovered ? kAccentSoft : kAccent)
                              : (m_hovered ? QColor(70, 78, 94) : kCardBorder);

    // 合并节点：外面多一圈浅色光带，一眼看出"这几个分类被捆在一起了"
    if (m_merged) {
        painter->setPen(QPen(QColor(76, 141, 255, 120), 1.0));
        painter->setBrush(QColor(76, 141, 255, 22));
        painter->drawRoundedRect(body.adjusted(-4, -4, 4, 4), 15, 15);
    }

    // 阴影
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 8, 20, m_active ? 90 : 50));
    painter->drawRoundedRect(body.translated(0, 2), 12, 12);

    // 卡片
    painter->setBrush(m_active ? kCardBg : kCardBgDim);
    painter->setPen(QPen(border, m_active ? 1.6 : 1.2));
    painter->drawRoundedRect(body, 12, 12);

    // 左侧层级色条
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_active ? kAccent : QColor(58, 64, 78));
    painter->drawRoundedRect(QRectF(4, 12, 3, kNodeH - 24), 1.5, 1.5);

    // 标题
    QFont font = painter->font();
    font.setBold(true);
    font.setPixelSize(14);
    painter->setFont(font);
    painter->setPen(m_active ? kTextMain : QColor(170, 176, 188));
    painter->drawText(QRectF(14, 8, kNodeW - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
                      elided(painter, title(), kNodeW - 26));

    // 副标题
    const QString sub = subtitle();
    if (!sub.isEmpty()) {
        font.setBold(false);
        font.setPixelSize(12);
        painter->setFont(font);
        painter->setPen(m_active ? kTextDim : QColor(112, 119, 132));
        painter->drawText(QRectF(14, 28, kNodeW - 24, 16), Qt::AlignLeft | Qt::AlignVCenter,
                          elided(painter, sub, kNodeW - 26));
    }

    // 端口
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_active ? kAccent : QColor(64, 71, 86));
    painter->drawEllipse(QPointF(1.5, kNodeH / 2.0), 3.2, 3.2);
    painter->drawEllipse(QPointF(kNodeW - 1.5, kNodeH / 2.0), 3.2, 3.2);
}

QVariant FlowNodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        // 连线画在背景层：节点移动后，旧位置的箭头不会被自动擦掉，
        // 拖动时就会残留成一串"锯齿"。这里整屏重绘一次，画布很小，代价可以忽略。
        if (QGraphicsScene *sc = scene())
            sc->update();
    }
    return QGraphicsObject::itemChange(change, value);
}
void FlowNodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_pressScenePos = event->scenePos();
        m_pressItemPos = pos();
        setZValue(20.0);
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void FlowNodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_dragging || !(event->buttons() & Qt::LeftButton)) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    QPointF target = m_pressItemPos + (event->scenePos() - m_pressScenePos);
    if (scene()) {
        const QRectF area = scene()->sceneRect();
        target.setX(qBound(area.left() - kNodeW / 2.0, target.x(),
                           qMax(area.left() - kNodeW / 2.0, area.right() - kNodeW / 2.0)));
        target.setY(qBound(0.0, target.y(), qMax(0.0, area.height() - kNodeH)));
    }
    setPos(target);
    event->accept();
}

void FlowNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        setZValue(1.0);
        if (auto *flowScene = qobject_cast<FlowScene *>(scene()))
            flowScene->finishNodeDrag(this);
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void FlowNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    // 双击合并节点：改分隔符
    if (event->button() == Qt::LeftButton && m_merged) {
        m_dragging = false;
        setZValue(1.0);
        if (auto *flowScene = qobject_cast<FlowScene *>(scene())) {
            flowScene->editMergedSeparator(this);
            event->accept();
            return;
        }
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

void FlowNodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;
    setCursor(Qt::OpenHandCursor);
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void FlowNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = false;
    unsetCursor();
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

/* ------------------------------------------------------------------ */
/* FlowScene                                                           */
/* ------------------------------------------------------------------ */

FlowScene::FlowScene(FlowCanvas *canvas, QObject *parent)
    : QGraphicsScene(parent)
    , m_canvas(canvas)
{
    setSceneRect(0, 0, m_canvasSize.width(), m_canvasSize.height());
    setItemIndexMethod(QGraphicsScene::NoIndex);
    setBackgroundBrush(Qt::NoBrush);
}

QStringList FlowScene::config() const
{
    QVector<FlowPath::Item> items;

    FlowNodeItem *start = nullptr;
    for (FlowNodeItem *node : m_nodes) {
        if (!m_prev.contains(node)) {
            start = node;
            break;
        }
    }

    QSet<FlowNodeItem *> visited;
    int groupId = 0;
    for (FlowNodeItem *node = start; node && !visited.contains(node);
         node = m_next.value(node, nullptr)) {
        visited.insert(node);
        const QVector<FlowPath::Item> members = node->members();
        if (members.isEmpty())
            continue;

        if (!node->isMerged()) {
            items.append(members.first());
            continue;
        }

        ++groupId;
        for (const FlowPath::Item &member : members) {
            FlowPath::Item item = member;
            item.joinSep = node->separator();
            item.groupId = groupId;
            items.append(item);
        }
    }

    return FlowPath::serialize(items);
}

void FlowScene::setConfig(const QStringList &config)
{
    m_loading = true;

    for (FlowNodeItem *node : m_nodes) {
        removeItem(node);
        node->deleteLater();
    }
    m_nodes.clear();
    m_next.clear();
    m_prev.clear();
    m_dropLevel = -1;

    // 同一 groupId 的连续分类项收成一个合并节点
    const QVector<FlowPath::Item> items = FlowPath::parse(config);
    QVector<FlowPath::Item> group;
    int groupId = 0;
    QString groupSep;

    auto flushGroup = [this, &group, &groupId, &groupSep]() {
        if (!group.isEmpty() && m_nodes.size() < kMaxLevels)
            appendMergedNode(group, groupSep);
        group.clear();
        groupId = 0;
        groupSep.clear();
    };

    for (const FlowPath::Item &item : items) {
        if (m_nodes.size() >= kMaxLevels)
            break;

        if (item.groupId > 0 && !item.joinSep.isEmpty()) {
            if (item.groupId != groupId) {
                flushGroup();
                groupId = item.groupId;
                groupSep = item.joinSep;
            }
            group.append(item);
            continue;
        }

        flushGroup();
        insertNode(FlowPath::keyName(item.key), item.text, -1);
    }
    flushGroup();

    m_loading = false;
    reindex();
    relayout(false);
    rebuildChain();
    update();
}

void FlowScene::appendMergedNode(const QVector<FlowPath::Item> &members, const QString &separator)
{
    auto *node = new FlowNodeItem(members, separator);
    addItem(node);
    m_nodes.append(node);

    // link() 会比较 level，所以先把新节点的层级排好，再连到前一个节点
    reindex();
    if (m_nodes.size() > 1)
        link(m_nodes.at(m_nodes.size() - 2), node);
}

void FlowScene::clearAll()
{
    setConfig(QStringList());
    notifyChanged();
}

void FlowScene::setCanvasSize(const QSize &size)
{
    if (size.width() <= 0 || size.height() <= 0)
        return;
    if (size == m_canvasSize)
        return;
    m_canvasSize = size;
    relayout(false);
    update();
}

bool FlowScene::containsKey(const QString &key) const
{
    for (const FlowNodeItem *node : m_nodes) {
        for (const FlowPath::Item &item : node->members()) {
            if (FlowPath::keyName(item.key) == key)
                return true;
        }
    }
    return false;
}

FlowNodeItem *FlowScene::nodeAt(const QPointF &scenePos) const
{
    const QList<QGraphicsItem *> hits = items(scenePos);
    for (QGraphicsItem *item : hits) {
        if (auto *node = qobject_cast<FlowNodeItem *>(item->toGraphicsObject()))
            return node;
    }
    return nullptr;
}

int FlowScene::levelForX(qreal x) const
{
    int level = 0;
    for (const FlowNodeItem *node : m_nodes) {
        if (x > node->pos().x() + kNodeW / 2.0)
            ++level;
    }
    return level;
}

void FlowScene::insertFromPalette(const QString &key, int level)
{
    const FlowPaletteItem *item = flowPaletteItem(key);
    if (!item)
        return;

    const bool allowDuplicate = (key == QStringLiteral("event") || key == QStringLiteral("custom"));
    if (!allowDuplicate && containsKey(key)) {
        emit statusMessage(QStringLiteral("「%1」已经在流程里了").arg(item->name));
        return;
    }
    if (m_nodes.size() >= kMaxLevels) {
        emit statusMessage(QStringLiteral("最多支持 %1 级目录").arg(kMaxLevels));
        return;
    }

    QString text;
    if (key == QStringLiteral("event") || key == QStringLiteral("custom")) {
        if (!m_canvas || !m_canvas->askNodeText(key, QString(), &text))
            return;
    }

    insertNode(key, text, level < 0 ? m_nodes.size() : level);
}

void FlowScene::addPaletteItemToNode(const QString &key, FlowNodeItem *target, bool before)
{
    const FlowPaletteItem *item = flowPaletteItem(key);
    if (!item || !target || !m_nodes.contains(target))
        return;

    if (key == QStringLiteral("seq")) {
        emit statusMessage(QStringLiteral("「序号」只影响文件名，不能合并进文件夹"));
        return;
    }

    const bool allowDuplicate = (key == QStringLiteral("event") || key == QStringLiteral("custom"));
    if (!allowDuplicate && containsKey(key)) {
        emit statusMessage(QStringLiteral("「%1」已经在流程里了").arg(item->name));
        return;
    }

    QString text;
    if (key == QStringLiteral("event") || key == QStringLiteral("custom")) {
        if (!m_canvas || !m_canvas->askNodeText(key, QString(), &text))
            return;
    }

    FlowPath::Item added;
    if (!FlowPath::keyFromName(key, &added.key))
        return;
    added.text = text;

    QVector<FlowPath::Item> members = target->members();
    if (members.size() >= kMaxLevels) {
        emit statusMessage(QStringLiteral("一个合并节点最多 %1 项").arg(kMaxLevels));
        return;
    }
    if (before)
        members.prepend(added);
    else
        members.append(added);

    QStringList sampleParts;
    for (const FlowPath::Item &member : members)
        sampleParts << sampleForItem(member);

    const QString initial = target->isMerged() ? target->separator()
                                               : SettingsManager::instance()->mergeSeparator();
    QString separator;
    if (!m_canvas || !m_canvas->askMergeSeparator(initial, sampleParts, &separator))
        return;

    const int index = m_nodes.indexOf(target);
    m_nodes.removeAll(target);
    m_prev.remove(target);
    m_next.remove(target);
    removeItem(target);
    target->deleteLater();

    auto *merged = new FlowNodeItem(members, separator);
    addItem(merged);
    m_nodes.insert(qBound(0, index, m_nodes.size()), merged);
    reindex();
    relinkByOrder();
    relayout(true);
    notifyChanged();
    emit statusMessage(QStringLiteral("已合并：%1").arg(merged->title()));
}

void FlowScene::insertNode(const QString &key, const QString &text, int level)
{
    auto *node = new FlowNodeItem(key, text);
    addItem(node);

    if (level < 0 || level > m_nodes.size())
        level = m_nodes.size();
    level = qBound(0, level, m_nodes.size());

    FlowNodeItem *left = (level > 0) ? m_nodes.at(level - 1) : nullptr;
    FlowNodeItem *right = (level < m_nodes.size()) ? m_nodes.at(level) : nullptr;

    m_nodes.insert(level, node);
    reindex();

    if (left) {
        link(left, node);
        if (right)
            link(node, right);
    } else if (right) {
        link(node, right);
    }

    node->setPos(QPointF(kMarginX + level * m_pitch, (m_canvasSize.height() - kNodeH) / 2.0));

    relayout(!m_loading);
    notifyChanged();

    // 新节点：淡入 + 轻微上浮，别让它"啪"地蹦出来
    if (!m_loading) {
        const qreal target = node->opacity();
        node->setOpacity(0.0);
        auto *pop = new QPropertyAnimation(node, "opacity", node);
        pop->setDuration(220);
        pop->setStartValue(0.0);
        pop->setEndValue(target);
        pop->setEasingCurve(QEasingCurve::OutCubic);
        pop->start();
    }
}

void FlowScene::removeNode(FlowNodeItem *node)
{
    if (!node || !m_nodes.contains(node))
        return;

    FlowNodeItem *prev = m_prev.value(node, nullptr);
    FlowNodeItem *next = m_next.value(node, nullptr);

    if (prev)
        unlinkRight(prev);
    unlinkRight(node);

    m_nodes.removeAll(node);
    m_prev.remove(node);
    m_next.remove(node);

    removeItem(node);
    node->deleteLater();

    // 删除中间节点时把前后接起来
    if (prev && next)
        link(prev, next);

    reindex();
    relayout(true);
    notifyChanged();
}

void FlowScene::reindex()
{
    for (int i = 0; i < m_nodes.size(); ++i)
        m_nodes[i]->setLevel(i);
}

void FlowScene::relayout(bool animate)
{
    // 防重入：relayout 里改 sceneRect 可能又触发一次布局
    if (m_layingOut)
        return;
    m_layingOut = true;
    struct Guard
    {
        bool *flag;
        ~Guard() { *flag = false; }
    } guard{&m_layingOut};

    // 注意：这里不能对 m_anims 里的裸指针直接 stop()——之前的动画是 DeleteWhenStopped，
    // 事件循环跑过一轮后对象已经被删掉，再访问就是野指针（表现为拖完一次就卡死/崩）。
    for (QPointer<QPropertyAnimation> &anim : m_anims) {
        if (!anim)
            continue;
        anim->stop();
        anim->deleteLater();
    }
    m_anims.clear();

    const qreal avail = qMax<qreal>(m_canvasSize.width(), 380);
    qreal pitch = kPitch;
    if (m_nodes.size() > 1) {
        const qreal needed = kMarginX * 2 + (m_nodes.size() - 1) * kPitch + kNodeW;
        if (needed > avail) {
            // 节点太多时压缩间距，尽量不出现横向滚动条
            const qreal fitted = (avail - kMarginX * 2 - kNodeW) / qreal(m_nodes.size() - 1);
            pitch = qBound<qreal>(kNodeW + 16, fitted, kPitch);
        }
    }
    m_pitch = pitch;

    const qreal contentWidth = m_nodes.isEmpty()
                                   ? avail
                                   : kMarginX * 2 + (m_nodes.size() - 1) * pitch + kNodeW;
    const qreal width = qMax(avail, contentWidth);
    const qreal height = qMax(qreal(kMinCanvasH), qreal(m_canvasSize.height()));
    setSceneRect(0, 0, width, height);

    const qreal targetY = qMax<qreal>(10.0, (height - kNodeH) / 2.0);
    const int duration = int(170 / animFactor());

    for (int i = 0; i < m_nodes.size(); ++i) {
        FlowNodeItem *node = m_nodes.at(i);
        const QPointF target(kMarginX + i * pitch, targetY);

        if (!animate || QLineF(node->pos(), target).length() < 0.5) {
            node->setPos(target);
            continue;
        }

        auto *anim = new QPropertyAnimation(node, "pos", this);
        anim->setDuration(qMax(60, duration));
        anim->setStartValue(node->pos());
        anim->setEndValue(target);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        // 动画结束后自己从列表里摘掉再销毁，避免列表里留着野指针
        connect(anim, &QPropertyAnimation::finished, this, [this, anim]() {
            m_anims.removeAll(anim);
            anim->deleteLater();
        });
        anim->start();
        m_anims.append(anim);
    }
}

void FlowScene::link(FlowNodeItem *from, FlowNodeItem *to)
{
    if (!from || !to || from == to)
        return;
    if (from->level() >= to->level())
        return;

    unlinkRight(from);
    if (FlowNodeItem *oldPrev = m_prev.value(to, nullptr))
        unlinkRight(oldPrev);

    m_next.insert(from, to);
    m_prev.insert(to, from);
}

void FlowScene::unlinkRight(FlowNodeItem *from)
{
    if (!from)
        return;
    FlowNodeItem *to = m_next.take(from);
    if (to)
        m_prev.remove(to);
}

void FlowScene::autoLinkAll()
{
    m_next.clear();
    m_prev.clear();
    for (int i = 0; i + 1 < m_nodes.size(); ++i)
        link(m_nodes.at(i), m_nodes.at(i + 1));
    notifyChanged();
    emit statusMessage(QStringLiteral("已按从左到右的顺序重新连线"));
}

void FlowScene::rebuildChain()
{
    for (FlowNodeItem *node : m_nodes)
        node->setChainActive(false);

    FlowNodeItem *start = nullptr;
    for (FlowNodeItem *node : m_nodes) {
        if (!m_prev.contains(node)) {
            start = node;
            break;
        }
    }

    QSet<FlowNodeItem *> visited;
    for (FlowNodeItem *node = start; node && !visited.contains(node);
         node = m_next.value(node, nullptr)) {
        visited.insert(node);
        node->setChainActive(true);
    }
}

void FlowScene::finishNodeDrag(FlowNodeItem *node)
{
    const int from = m_nodes.indexOf(node);
    if (from < 0)
        return;

    // 拖到另一个节点身上 → 合并（弹窗里选分隔符）
    if (FlowNodeItem *target = mergeTargetAt(node)) {
        mergeNodes(node, target);
        return;
    }

    const qreal centerX = node->pos().x() + kNodeW / 2.0;
    int to = 0;
    for (FlowNodeItem *other : m_nodes) {
        if (other == node)
            continue;
        if (centerX > other->pos().x() + kNodeW / 2.0)
            ++to;
    }

    if (to != from) {
        m_nodes.removeAt(from);
        m_nodes.insert(to, node);
        reindex();

        // 顺序变了：按新顺序重新连成一条链，避免出现悬空/反向的连线
        relinkByOrder();

        QStringList names;
        for (const FlowNodeItem *n : m_nodes)
            names << n->title();
        emit statusMessage(QStringLiteral("已调整顺序：%1").arg(names.join(QStringLiteral(" → "))));
    }

    relayout(true);
    notifyChanged();
}

FlowNodeItem *FlowScene::mergeTargetAt(FlowNodeItem *node) const
{
    if (!node)
        return nullptr;

    const QRectF self(node->pos(), QSizeF(kNodeW, kNodeH));
    const qreal area = kNodeW * kNodeH;
    FlowNodeItem *best = nullptr;
    qreal bestArea = 0.0;

    for (FlowNodeItem *other : m_nodes) {
        if (other == node)
            continue;
        const QRectF otherRect(other->pos(), QSizeF(kNodeW, kNodeH));
        const QRectF overlap = self.intersected(otherRect);
        const qreal covered = overlap.width() * overlap.height();
        // 重叠超过 30% 才算"拖到它身上"，避免只是轻轻擦过
        if (covered >= area * 0.30 && covered > bestArea) {
            best = other;
            bestArea = covered;
        }
    }
    return best;
}

void FlowScene::mergeNodes(FlowNodeItem *dragged, FlowNodeItem *target)
{
    if (!dragged || !target || dragged == target) {
        relayout(true);
        return;
    }

    const QVector<FlowPath::Item> draggedMembers = dragged->members();
    const QVector<FlowPath::Item> targetMembers = target->members();

    auto hasSeq = [](const QVector<FlowPath::Item> &members) {
        for (const FlowPath::Item &item : members) {
            if (item.key == FlowPath::Key::Seq)
                return true;
        }
        return false;
    };
    if (hasSeq(draggedMembers) || hasSeq(targetMembers)) {
        emit statusMessage(QStringLiteral("「序号」只影响文件名，不能合并进文件夹"));
        relayout(true);
        return;
    }

    // 成员顺序按流程图里原来的先后（拖动不会改变流程顺序）
    const bool draggedFirst = dragged->level() <= target->level();
    QVector<FlowPath::Item> members;
    for (const FlowPath::Item &item : (draggedFirst ? draggedMembers : targetMembers))
        members.append(item);
    for (const FlowPath::Item &item : (draggedFirst ? targetMembers : draggedMembers))
        members.append(item);

    if (members.size() > kMaxLevels) {
        emit statusMessage(QStringLiteral("一个合并节点最多 %1 项").arg(kMaxLevels));
        relayout(true);
        return;
    }

    // 先恢复布局再弹窗：弹窗期间节点不会停在别的节点身上
    relayout(true);

    QPointer<FlowNodeItem> draggedGuard(dragged);
    QPointer<FlowNodeItem> targetGuard(target);
    const QString initial = target->isMerged() ? target->separator()
                                               : SettingsManager::instance()->mergeSeparator();
    QStringList sampleParts;
    for (const FlowPath::Item &item : members)
        sampleParts << sampleForItem(item);

    QTimer::singleShot(0, this, [this, draggedGuard, targetGuard, members, sampleParts, initial]() {
        if (!draggedGuard || !targetGuard || !m_nodes.contains(draggedGuard)
            || !m_nodes.contains(targetGuard)) {
            return;
        }

        QString separator;
        if (!m_canvas || !m_canvas->askMergeSeparator(initial, sampleParts, &separator)) {
            relayout(true);
            return;
        }

        FlowNodeItem *first = draggedGuard;
        FlowNodeItem *second = targetGuard;
        const int index = qMin(m_nodes.indexOf(first), m_nodes.indexOf(second));

        for (FlowNodeItem *node : {first, second}) {
            m_nodes.removeAll(node);
            m_prev.remove(node);
            m_next.remove(node);
            removeItem(node);
            node->deleteLater();
        }

        auto *merged = new FlowNodeItem(members, separator);
        addItem(merged);
        m_nodes.insert(qBound(0, index, m_nodes.size()), merged);
        reindex();
        relinkByOrder();
        relayout(true);
        notifyChanged();
        emit statusMessage(QStringLiteral("已合并：%1").arg(merged->title()));
    });
}

void FlowScene::unmergeNode(FlowNodeItem *node)
{
    if (!node || !node->isMerged() || !m_nodes.contains(node))
        return;

    const int index = m_nodes.indexOf(node);
    const QVector<FlowPath::Item> members = node->members();
    if (members.size() < 2) {
        relayout(true);
        return;
    }

    m_nodes.removeAll(node);
    m_prev.remove(node);
    m_next.remove(node);
    removeItem(node);
    node->deleteLater();

    int insertAt = qBound(0, index, m_nodes.size());
    for (const FlowPath::Item &item : members) {
        auto *single = new FlowNodeItem(FlowPath::keyName(item.key), item.text);
        addItem(single);
        m_nodes.insert(insertAt++, single);
    }

    reindex();
    relinkByOrder();
    relayout(true);
    notifyChanged();
    emit statusMessage(QStringLiteral("已拆开合并节点"));
}

void FlowScene::removeNodeWithConfirm(FlowNodeItem *node)
{
    if (!node || !m_nodes.contains(node))
        return;

    const QString title = node->title();
    if (!DarkDialog::confirmTwice(m_canvas, QStringLiteral("删除节点"),
                                  QStringLiteral("确定要删除节点「%1」吗？").arg(title),
                                  QStringLiteral("删除"))) {
        return;
    }

    const QStringList snapshot = config();
    removeNode(node);

    QPointer<FlowScene> guard(this);
    Toast::showToast(QStringLiteral("已删除节点"), title, Toast::Success, 10000,
                     QStringLiteral("撤销"), [guard, snapshot]() {
                         if (!guard)
                             return;
                         guard->setConfig(snapshot);
                         guard->notifyChanged();
                     });
}

void FlowScene::restoreLinks(const QVector<QPair<int, int>> &links)
{
    m_next.clear();
    m_prev.clear();

    for (const QPair<int, int> &pair : links) {
        FlowNodeItem *from = nullptr;
        FlowNodeItem *to = nullptr;
        for (FlowNodeItem *node : m_nodes) {
            if (node->level() == pair.first)
                from = node;
            else if (node->level() == pair.second)
                to = node;
        }
        if (from && to)
            link(from, to);
    }

    notifyChanged();
}

void FlowScene::relinkByOrder()
{
    m_next.clear();
    m_prev.clear();
    for (int i = 0; i + 1 < m_nodes.size(); ++i)
        link(m_nodes.at(i), m_nodes.at(i + 1));
}

void FlowScene::editMergedSeparator(FlowNodeItem *node)
{
    if (!node || !node->isMerged())
        return;

    QString separator;
    if (!m_canvas
        || !m_canvas->askMergeSeparator(node->separator(), node->sampleParts(), &separator)) {
        return;
    }

    node->setSeparator(separator);
    notifyChanged();
    emit statusMessage(QStringLiteral("分隔符已改成「%1」")
                           .arg(separator == QStringLiteral(" ") ? QStringLiteral("空格")
                                                                 : separator));
}

void FlowScene::notifyChanged()
{
    rebuildChain();
    update();
    if (!m_loading)
        emit changed();
}

/* ---------------- 绘制 ---------------- */

namespace {

// 拖动节点时用的轻量直线连线：方向随意、不会打结，也不会盖住节点
void drawDraggingLink(QPainter *painter, const QPointF &from, const QPointF &to, bool active)
{
    const QPointF start = from + QPointF(4.0, 0);
    const QPointF tip = to - QPointF(4.0, 0);
    const QPointF delta = tip - start;
    const qreal length = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    if (length < 8.0)
        return;

    const QPointF dir = delta / length;
    const QPointF base = tip - dir * 8.0;
    const QPointF normal(-dir.y(), dir.x());

    QPen pen(active ? kAccent : kLinkIdle, 1.4);
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(start, base);

    QPolygonF head;
    head << tip << (base + normal * 4.0) << (base - normal * 4.0);
    painter->setPen(Qt::NoPen);
    painter->setBrush(active ? kAccent : kLinkIdle);
    painter->drawPolygon(head);
}

} // namespace

void FlowScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QRectF box = sceneRect().adjusted(1, 1, -1, -1);
    QPainterPath panel;
    panel.addRoundedRect(box, 12, 12);

    painter->fillPath(panel, kCanvasBg);

    painter->save();
    painter->setClipPath(panel);
    painter->setPen(QPen(QColor(34, 39, 50)));

    // 只画露出来的那部分点阵，避免整幅重绘时白跑一遍循环
    const QRectF gridArea = rect.intersected(box);
    if (!gridArea.isEmpty()) {
        const qreal step = 20.0;
        const qreal originX = box.left() + 18.0;
        const qreal originY = box.top() + 18.0;
        const qreal startX = originX + qCeil((gridArea.left() - originX) / step) * step;
        const qreal startY = originY + qCeil((gridArea.top() - originY) / step) * step;

        QVector<QPointF> dots;
        const int cols = int(qMax<qreal>(0, (gridArea.right() - startX) / step)) + 1;
        const int rows = int(qMax<qreal>(0, (gridArea.bottom() - startY) / step)) + 1;
        if (qint64(cols) * qint64(rows) <= 40000) {
            dots.reserve(cols * rows);
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c)
                    dots.append(QPointF(startX + c * step, startY + r * step));
            }
            if (!dots.isEmpty())
                painter->drawPoints(dots.constData(), dots.size());
        }
    }
    painter->restore();

    // 连线画在节点下面：即使位置有重叠，也会被节点卡片盖住，不会糊在边框上
    for (auto it = m_next.constBegin(); it != m_next.constEnd(); ++it) {
        FlowNodeItem *from = it.key();
        FlowNodeItem *to = it.value();
        if (!from || !to)
            continue;
        const bool inChain = from->chainActive() && to->chainActive();
        if (from->isDragging() || to->isDragging())
            drawDraggingLink(painter, from->rightPort(), to->leftPort(), inChain);
        else
            paintArrow(painter, from->rightPort(), to->leftPort(), inChain, !inChain);
    }

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(kCardBorder, 1));
    painter->drawPath(panel);
}

void FlowScene::paintArrow(QPainter *painter, const QPointF &from, const QPointF &to,
                           bool active, bool dashed)
{
    Q_UNUSED(dashed)

    const qreal gap = 6.0;       // 离节点边缘留的空隙，箭头不会糊在边框上
    const qreal headLen = 9.0;
    const qreal headHalf = 4.6;

    const QPointF start = from + QPointF(gap, 0);
    const QPointF tip = to - QPointF(gap - 2.0, 0);

    const qreal spanX = tip.x() - start.x();
    const qreal spanY = tip.y() - start.y();

    QPainterPath path;

    if (spanX >= headLen + 10.0) {
        const QPointF base = tip - QPointF(headLen, 0);
        path.moveTo(start);

        if (qAbs(spanY) < 1.2) {
            path.lineTo(base);
        } else {
            // 折线走法（圆角）：永远不会有控制点交叉打结
            qreal midX = start.x() + qBound<qreal>(16.0, spanX * 0.5, 44.0);
            midX = qMin(midX, tip.x() - headLen - 10.0);
            midX = qMax(midX, start.x() + 10.0);
            const qreal radius = qMin<qreal>(9.0, qMin(qAbs(spanY) * 0.5, qMax<qreal>(1.0, qMin(midX - start.x(), tip.x() - midX) * 0.45)));
            const qreal dir = spanY > 0.0 ? 1.0 : -1.0;

            path.lineTo(QPointF(midX - radius, start.y()));
            path.quadTo(QPointF(midX, start.y()),
                        QPointF(midX, start.y() + dir * radius));
            path.lineTo(QPointF(midX, tip.y() - dir * radius));
            path.quadTo(QPointF(midX, tip.y()), QPointF(midX + radius, tip.y()));
            path.lineTo(base);
        }
    } else if (spanX > 4.0) {
        // 节点挨太近：画一小段直线就够了
        path.moveTo(start);
        path.lineTo(tip - QPointF(headLen * 0.6, 0));
    } else {
        // 目标在左边或者几乎重叠：从下方绕一圈，避免线穿节点
        const qreal detourY = qMax(start.y(), tip.y()) + 36.0;
        const qreal lead = 16.0;
        path.moveTo(start);
        path.lineTo(start + QPointF(lead, 0));
        path.lineTo(QPointF(start.x() + lead, detourY));
        path.lineTo(QPointF(tip.x() - lead, detourY));
        path.lineTo(QPointF(tip.x() - lead, tip.y()));
        path.lineTo(tip - QPointF(headLen, 0));
    }

    // 底部柔光，让连线有点"发光"的感觉
    if (active) {
        QPen glowPen(QColor(kAccent.red(), kAccent.green(), kAccent.blue(), 34), 7);
        glowPen.setCapStyle(Qt::RoundCap);
        glowPen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(glowPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }

    QPen pen(active ? kAccent : kLinkIdle, active ? 2.0 : 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);

    // 箭头（尖端正好落在节点前面）
    const QPointF headBase = tip - QPointF(headLen, 0);
    QPolygonF head;
    head << tip << QPointF(headBase.x() + 1.0, headBase.y() - headHalf)
         << QPointF(headBase.x() + 1.0, headBase.y() + headHalf);

    painter->setPen(Qt::NoPen);
    painter->setBrush(active ? kAccent : kLinkIdle);
    painter->drawPolygon(head);
}

void FlowScene::drawForeground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect)

    painter->setRenderHint(QPainter::Antialiasing, true);

    // 正在拉的连线
    if (m_connectFrom) {
        const QPointF from = m_connectFrom->rightPort() + QPointF(6, 0);
        const QPointF to = m_connectPoint;

        QPainterPath path(from);
        const qreal spanX = to.x() - from.x();
        const qreal spanY = to.y() - from.y();
        if (spanX > 12.0 && qAbs(spanY) >= 1.2) {
            const qreal midX = from.x() + spanX * 0.5;
            const qreal radius = qMin<qreal>(9.0, qMin(qAbs(spanY) * 0.5, spanX * 0.22));
            const qreal dir = spanY > 0 ? 1.0 : -1.0;
            path.lineTo(QPointF(midX - radius, from.y()));
            path.quadTo(QPointF(midX, from.y()), QPointF(midX, from.y() + dir * radius));
            path.lineTo(QPointF(midX, to.y() - dir * radius));
            path.quadTo(QPointF(midX, to.y()), QPointF(midX + radius, to.y()));
            path.lineTo(to);
        } else if (spanX > 12.0) {
            path.lineTo(to);
        } else {
            const qreal detourY = qMax(from.y(), to.y()) + 36.0;
            path.lineTo(from + QPointF(16, 0));
            path.lineTo(QPointF(from.x() + 16, detourY));
            path.lineTo(QPointF(to.x() - 16, detourY));
            path.lineTo(QPointF(to.x() - 16, to.y()));
            path.lineTo(to);
        }

        painter->setPen(QPen(kAccentSoft, 2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);

        painter->setPen(Qt::NoPen);
        painter->setBrush(kAccentSoft);
        painter->drawEllipse(to, 3.5, 3.5);
    }

    // 拖分类项到节点上：把那个节点整体圈出来，松手就合并进去
    if (m_dropTarget) {
        const QRectF box(m_dropTarget->pos(), QSizeF(kNodeW, kNodeH));
        painter->setPen(QPen(kAccentSoft, 2.0));
        painter->setBrush(QColor(76, 141, 255, 30));
        painter->drawRoundedRect(box.adjusted(-5, -5, 5, 5), 15, 15);

        QFont font = painter->font();
        font.setPixelSize(12);
        painter->setFont(font);
        painter->setPen(kAccentSoft);
        painter->drawText(box.translated(0, -20), Qt::AlignCenter,
                          QStringLiteral("松手合并进这个节点"));
    }

    // 拖入位置指示
    if (m_dropLevel >= 0) {
        qreal x = kMarginX - 12;
        if (m_dropLevel > 0) {
            const qreal rightEdge = kMarginX + (m_dropLevel - 1) * m_pitch + kNodeW;
            x = rightEdge + (m_pitch - kNodeW) / 2.0;
        }
        painter->setPen(QPen(kAccent, 2, Qt::DashLine));
        painter->drawLine(QPointF(x, 14), QPointF(x, sceneRect().height() - 14));
    }

    // 空画布提示
    if (m_nodes.isEmpty()) {
        QFont font = painter->font();
        font.setPixelSize(14);
        painter->setFont(font);
        painter->setPen(kTextMuted);
        const qreal width = qMin(sceneRect().width(), qreal(m_canvasSize.width()));
        painter->drawText(QRectF(0, 0, width, sceneRect().height()), Qt::AlignCenter,
                          QStringLiteral("把上面的分类项拖到这里，从左到右排成一条流程\n"
                                         "把一个节点拖到另一个节点上可以合并成一层文件夹\n"
                                         "右键拖到右侧节点可连线，右键节点可改名 / 删除"));
    }
}

/* ---------------- 拖放 ---------------- */

void FlowScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasFormat(FlowMime::Format)) {
        updateDropIndicator(event->scenePos());
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragEnterEvent(event);
}

void FlowScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasFormat(FlowMime::Format)) {
        updateDropIndicator(event->scenePos());
        event->acceptProposedAction();
        return;
    }
    QGraphicsScene::dragMoveEvent(event);
}

void FlowScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (m_dropLevel >= 0 || m_dropTarget) {
        m_dropLevel = -1;
        m_dropTarget = nullptr;
        update();
    }
    QGraphicsScene::dragLeaveEvent(event);
}

void FlowScene::updateDropIndicator(const QPointF &scenePos)
{
    FlowNodeItem *target = nodeAt(scenePos);
    if (target != m_dropTarget) {
        m_dropTarget = target;
        update();
    }

    // 悬停在节点上时不再画竖线，改用节点高亮
    const int level = target ? -1 : levelForX(scenePos.x());
    if (level != m_dropLevel) {
        m_dropLevel = level;
        update();
    }
}

void FlowScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    const QString key = QString::fromUtf8(event->mimeData()->data(FlowMime::Format));
    const int level = m_dropLevel;
    FlowNodeItem *target = m_dropTarget;
    m_dropLevel = -1;
    m_dropTarget = nullptr;
    update();

    if (key.isEmpty()) {
        QGraphicsScene::dropEvent(event);
        return;
    }
    event->acceptProposedAction();

    // 直接拖到某个节点身上：合并进去（不用先放到画布再拖第二次）
    if (target) {
        const bool before = event->scenePos().x() < target->pos().x() + kNodeW / 2.0;
        QPointer<FlowNodeItem> guard(target);
        QTimer::singleShot(0, this, [this, key, guard, before]() {
            if (guard && m_nodes.contains(guard))
                addPaletteItemToNode(key, guard, before);
        });
        return;
    }

    // 等拖放操作结束后再插入（插入可能要弹对话框）
    QTimer::singleShot(0, this, [this, key, level]() {
        insertFromPalette(key, level < 0 ? m_nodes.size() : level);
    });
}

/* ---------------- 鼠标 / 右键 ---------------- */

void FlowScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if (FlowNodeItem *node = nodeAt(event->scenePos())) {
            m_connectFrom = node;
            m_connectPoint = event->scenePos();
            m_connectDragging = false;
            event->accept();
            return;
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void FlowScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_connectFrom && (event->buttons() & Qt::RightButton)) {
        m_connectPoint = event->scenePos();
        if (QLineF(event->buttonDownScenePos(Qt::RightButton), m_connectPoint).length() > 4.0)
            m_connectDragging = true;
        update();
        event->accept();
        return;
    }
    QGraphicsScene::mouseMoveEvent(event);
}

void FlowScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_connectFrom && event->button() == Qt::RightButton) {
        FlowNodeItem *from = m_connectFrom;
        m_connectFrom = nullptr;
        m_suppressContextMenu = true;
        update();

        if (m_connectDragging) {
            FlowNodeItem *to = nodeAt(event->scenePos());
            if (!to || to == from) {
                emit statusMessage(QStringLiteral("把连线拖到右侧的节点上再松手"));
            } else if (to->level() <= from->level()) {
                emit statusMessage(QStringLiteral("连线只能从左到右"));
            } else {
                link(from, to);
                notifyChanged();
                emit statusMessage(QStringLiteral("已连线：%1 → %2").arg(from->title(), to->title()));
            }
        } else {
            showNodeMenu(from, event->screenPos());
        }
        m_connectDragging = false;
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

void FlowScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    if (m_suppressContextMenu) {
        m_suppressContextMenu = false;
        event->accept();
        return;
    }
    if (nodeAt(event->scenePos())) {
        event->accept();
        return;
    }
    showCanvasMenu(event->screenPos());
    event->accept();
}

/* ---------------- 菜单 ---------------- */

void FlowScene::showNodeMenu(FlowNodeItem *node, const QPoint &screenPos)
{
    if (!node)
        return;

    QMenu menu;

    if (node->isMerged()) {
        QAction *sepAct = menu.addAction(QStringLiteral("改分隔符…"));
        connect(sepAct, &QAction::triggered, this, [this, node]() { editMergedSeparator(node); });

        // 合并节点里的「事件 / 自定义」也能改名
        const QVector<FlowPath::Item> members = node->members();
        for (int i = 0; i < members.size(); ++i) {
            const FlowPath::Item member = members.at(i);
            if (member.key != FlowPath::Key::Event && member.key != FlowPath::Key::Custom)
                continue;
            const QString key = FlowPath::keyName(member.key);
            QAction *rename = menu.addAction(
                QStringLiteral("重命名「%1」…").arg(itemTitle(member)));
            connect(rename, &QAction::triggered, this, [this, node, key, i, member]() {
                QString text = member.text;
                if (m_canvas && m_canvas->askNodeText(key, text, &text)) {
                    node->setMemberText(i, text);
                    notifyChanged();
                }
            });
        }

        menu.addSeparator();
        QAction *splitAct = menu.addAction(QStringLiteral("拆开合并节点"));
        connect(splitAct, &QAction::triggered, this, [this, node]() { unmergeNode(node); });

        menu.addSeparator();
        QAction *removeAct = menu.addAction(QStringLiteral("删除节点"));
        connect(removeAct, &QAction::triggered, this,
                [this, node]() { removeNodeWithConfirm(node); });

        menu.exec(screenPos);
        return;
    }

    if (node->key() == QStringLiteral("event") || node->key() == QStringLiteral("custom")) {
        QAction *rename = menu.addAction(QStringLiteral("重命名…"));
        connect(rename, &QAction::triggered, this, [this, node]() {
            QString text = node->text();
            if (m_canvas && m_canvas->askNodeText(node->key(), text, &text)) {
                node->setText(text);
                notifyChanged();
            }
        });
        menu.addSeparator();
    }

    QMenu *insertMenu = menu.addMenu(QStringLiteral("在右侧插入…"));
    for (const FlowPaletteItem &item : flowPaletteItems()) {
        QAction *act = insertMenu->addAction(item.name);
        const QString key = item.key;
        connect(act, &QAction::triggered, this, [this, node, key]() {
            insertFromPalette(key, node->level() + 1);
        });
    }

    menu.addSeparator();
    QAction *unlinkAct = menu.addAction(QStringLiteral("断开右侧连线"));
    unlinkAct->setEnabled(m_next.contains(node));
    connect(unlinkAct, &QAction::triggered, this, [this, node]() {
        unlinkRight(node);
        notifyChanged();
        emit statusMessage(QStringLiteral("已断开连线"));
    });

    QAction *unlinkAllAct = menu.addAction(QStringLiteral("断开全部连线"));
    connect(unlinkAllAct, &QAction::triggered, this, [this]() {
        QVector<QPair<int, int>> links;
        for (auto it = m_next.constBegin(); it != m_next.constEnd(); ++it)
            links.append({it.key()->level(), it.value()->level()});

        if (!DarkDialog::confirmTwice(
                m_canvas, QStringLiteral("断开全部连线"),
                QStringLiteral("所有节点会变成互不相连，目录预览里就只剩连成一条链的部分。确定要断开吗？"),
                QStringLiteral("全部断开"))) {
            return;
        }

        m_next.clear();
        m_prev.clear();
        notifyChanged();
        emit statusMessage(QStringLiteral("已断开全部连线"));

        QPointer<FlowScene> guard(this);
        Toast::showToast(QStringLiteral("已断开全部连线"),
                         QStringLiteral("原来的连线已经全部取消"), Toast::Success, 10000,
                         QStringLiteral("撤销"), [guard, links]() {
                             if (guard)
                                 guard->restoreLinks(links);
                         });
    });

    menu.addSeparator();
    QAction *removeAct = menu.addAction(QStringLiteral("删除节点"));
    connect(removeAct, &QAction::triggered, this,
            [this, node]() { removeNodeWithConfirm(node); });

    menu.exec(screenPos);
}

void FlowScene::showCanvasMenu(const QPoint &screenPos)
{
    QMenu menu;
    QAction *relink = menu.addAction(QStringLiteral("按顺序重新连线"));
    relink->setEnabled(m_nodes.size() > 1);
    connect(relink, &QAction::triggered, this, &FlowScene::autoLinkAll);

    QAction *clear = menu.addAction(QStringLiteral("清空画布"));
    clear->setEnabled(!m_nodes.isEmpty());
    connect(clear, &QAction::triggered, this, [this]() {
        if (m_canvas)
            m_canvas->clearAll();
        else
            clearAll();
    });

    menu.exec(screenPos);
}

/* ------------------------------------------------------------------ */
/* FlowView                                                            */
/* ------------------------------------------------------------------ */

FlowView::FlowView(QWidget *parent)
    : QGraphicsView(parent)
{
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
    // 固定不要横向滚动条：它能和 resizeEvent 互相触发，转成死循环（表现为整个窗口未响应）
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setRenderHint(QPainter::Antialiasing, true);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setDragMode(QGraphicsView::NoDrag);
    setResizeAnchor(QGraphicsView::NoAnchor);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setStyleSheet(QStringLiteral("QGraphicsView { background: transparent; border: none; }"));
}

void FlowView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);

    // 不要在 resizeEvent 里同步改 sceneRect：滚动条/布局会因此再触发一次 resize，
    // 两个尺寸来回切就是"卡死"。改成合并到下一轮事件循环里应用一次。
    if (m_sizeScheduled)
        return;
    m_sizeScheduled = true;
    QTimer::singleShot(0, this, [this]() { applyCanvasSize(); });
}

void FlowView::applyCanvasSize()
{
    m_sizeScheduled = false;
    if (auto *flowScene = qobject_cast<FlowScene *>(scene()))
        flowScene->setCanvasSize(viewport()->size());
}

/* ------------------------------------------------------------------ */
/* FlowChip                                                            */
/* ------------------------------------------------------------------ */

FlowChip::FlowChip(const FlowPaletteItem &item, QWidget *parent)
    : QLabel(item.name, parent)
    , m_item(item)
{
    setObjectName(QStringLiteral("flowChip"));
    setAlignment(Qt::AlignCenter);
    setFixedSize(56, 28);
    setCursor(Qt::OpenHandCursor);
    setToolTip(QStringLiteral("%1\n%2\n\n拖到下方画布，或双击直接添加").arg(item.name, item.hint));
}

void FlowChip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->pos();
        m_mayDrag = true;
        // 必须接受按下事件，否则拿不到后续 mouseMoveEvent，拖拽永远起不来
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void FlowChip::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_mayDrag || !(event->buttons() & Qt::LeftButton))
        return;
    if ((event->pos() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
        return;

    m_mayDrag = false;

    auto *mime = new QMimeData();
    mime->setData(FlowMime::Format, m_item.key.toUtf8());
    mime->setText(m_item.name);

    const QPixmap snapshot = grab();
    QPixmap faded(snapshot.size());
    faded.fill(Qt::transparent);
    {
        QPainter painter(&faded);
        painter.setOpacity(0.85);
        painter.drawPixmap(0, 0, snapshot);
    }

    QDrag drag(this);
    drag.setMimeData(mime);
    drag.setPixmap(faded);
    drag.setHotSpot(m_pressPos);
    drag.exec(Qt::CopyAction);
}

void FlowChip::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit addRequested(m_item.key);
        event->accept();
        return;
    }
    QLabel::mouseDoubleClickEvent(event);
}

/* ------------------------------------------------------------------ */
/* FlowCanvas                                                          */
/* ------------------------------------------------------------------ */

FlowCanvas::FlowCanvas(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    // 分类项面板
    auto *palette = new QHBoxLayout();
    palette->setContentsMargins(0, 0, 0, 0);
    palette->setSpacing(6);

    for (const FlowPaletteItem &item : flowPaletteItems()) {
        auto *chip = new FlowChip(item, this);
        palette->addWidget(chip);
        connect(chip, &FlowChip::addRequested, this, &FlowCanvas::addPaletteItem);
    }
    palette->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("flowStatus"));
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setMaximumWidth(240);
    palette->addWidget(m_statusLabel);

    auto *clearBtn = new QPushButton(QStringLiteral("清空"), this);
    clearBtn->setObjectName(QStringLiteral("flowClearBtn"));
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setToolTip(QStringLiteral("清空画布上的所有分类项"));
    connect(clearBtn, &QPushButton::clicked, this, &FlowCanvas::clearAll);
    palette->addWidget(clearBtn);

    root->addLayout(palette);

    // 画布
    m_scene = new FlowScene(this, this);
    m_view = new FlowView(this);
    m_view->setScene(m_scene);
    m_view->setMinimumHeight(126);
    m_view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_view, 1);

    // 目录预览
    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName(QStringLiteral("flowPreview"));
    m_previewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_previewLabel);

    connect(m_scene, &FlowScene::changed, this, [this]() {
        updatePreview();
        emit flowChanged(m_scene->config());
    });
    connect(m_scene, &FlowScene::statusMessage, this, &FlowCanvas::showStatus);

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    m_statusTimer->setInterval(3600);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() { m_statusLabel->clear(); });

    setMinimumHeight(188);
    updatePreview();
}

QSize FlowCanvas::sizeHint() const
{
    return QSize(520, 202);
}

QStringList FlowCanvas::flowConfig() const
{
    return m_scene->config();
}

void FlowCanvas::setFlowConfig(const QStringList &config)
{
    m_scene->setConfig(config);
    updatePreview();
}

void FlowCanvas::clearAll()
{
    if (m_scene->nodeCount() == 0)
        return;

    const QStringList snapshot = m_scene->config();
    if (!DarkDialog::confirmTwice(this, QStringLiteral("清空画布"),
                                  QStringLiteral("画布上的所有分类项都会被移除，确定要清空吗？"),
                                  QStringLiteral("清空画布"))) {
        return;
    }

    m_scene->clearAll();
    showStatus(QStringLiteral("画布已清空"));

    QPointer<FlowCanvas> guard(this);
    Toast::showToast(QStringLiteral("画布已清空"),
                     QStringLiteral("原来的 %1 个分类项已经移除").arg(snapshot.size()),
                     Toast::Success, 10000, QStringLiteral("撤销"), [guard, snapshot]() {
                         if (guard)
                             guard->restoreConfig(snapshot);
                     });
}

void FlowCanvas::restoreConfig(const QStringList &config)
{
    setFlowConfig(config);
    emit flowChanged(flowConfig());
}

void FlowCanvas::addPaletteItem(const QString &key)
{
    m_scene->insertFromPalette(key, -1);
}

bool FlowCanvas::askNodeText(const QString &key, const QString &initial, QString *text)
{
    if (!text)
        return false;

    if (key == QStringLiteral("event")) {
        const QStringList events = SettingsManager::instance()->events();
        QString current = initial.trimmed();
        if (current.isEmpty())
            current = events.isEmpty() ? QStringLiteral("事件") : events.first();

        bool ok = false;
        QString name;
        if (events.isEmpty()) {
            name = QInputDialog::getText(this, QStringLiteral("事件分类"),
                                         QStringLiteral("事件名称（会成为文件夹名）:"),
                                         QLineEdit::Normal, current, &ok);
        } else {
            int index = events.indexOf(current);
            if (index < 0)
                index = 0;
            name = QInputDialog::getItem(this, QStringLiteral("事件分类"),
                                         QStringLiteral("选择或输入事件名称:"),
                                         events, index, true, &ok);
        }
        if (!ok)
            return false;
        name = name.trimmed();
        if (name.isEmpty())
            return false;
        SettingsManager::instance()->addEvent(name);
        *text = name;
        return true;
    }

    bool ok = false;
    const QString value = QInputDialog::getText(
        this, QStringLiteral("自定义文件夹"), QStringLiteral("文件夹名称:"),
        QLineEdit::Normal, initial.isEmpty() ? QStringLiteral("旅行") : initial, &ok);
    if (!ok)
        return false;
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return false;
    *text = trimmed;
    return true;
}

bool FlowCanvas::askMergeSeparator(const QString &initial, const QStringList &sampleParts,
                                   QString *separator)
{
    return SeparatorDialog::choose(this, initial, sampleParts, separator);
}

void FlowCanvas::updatePreview()
{
    const QString path = FlowPath::preview(m_scene->config(), cameraHintModel());
    if (path.isEmpty()) {
        m_previewLabel->setText(QStringLiteral("目录预览：（流程为空，文件直接放入目标文件夹）"));
        m_previewLabel->setToolTip(QString());
        return;
    }
    m_previewLabel->setText(QStringLiteral("目录预览：%1").arg(path));
    m_previewLabel->setToolTip(path);
}

void FlowCanvas::refreshPreview()
{
    updatePreview();
}

void FlowCanvas::setCameraModelHint(const QString &model)
{
    const QString trimmed = model.trimmed();
    if (trimmed.isEmpty() || cameraHintModel() == trimmed)
        return;

    cameraHintModel() = trimmed;
    if (m_scene)
        m_scene->update();   // 相机型号节点的文字跟着变
    updatePreview();
}

void FlowCanvas::showStatus(const QString &message)
{
    if (message.isEmpty()) {
        m_statusLabel->clear();
        m_statusLabel->setToolTip(QString());
        return;
    }

    // 状态栏宽度有限：超长消息省略显示，完整内容放 tooltip，避免把整页撑宽
    const int width = 224;
    m_statusLabel->setText(m_statusLabel->fontMetrics().elidedText(message, Qt::ElideRight, width));
    m_statusLabel->setToolTip(message);
    m_statusTimer->start();
}
