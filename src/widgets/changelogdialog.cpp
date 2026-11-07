#include "widgets/squircle.h"
#include "widgets/changelogdialog.h"
#include "core/changelog.h"
#include "core/version.h"

#include <QEnterEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dwmapi.h>
#endif

namespace {

const QColor kTitle(0xFF, 0xFF, 0xFF);
const QColor kDim(0x7C, 0x86, 0x98);
const QColor kHoverBg(0x1E, 0x24, 0x31);

void applyDarkTitleBar(QWidget *window)
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
#else
    Q_UNUSED(window)
#endif
}

QColor kindColor(Changelog::Kind kind)
{
    switch (kind) {
    case Changelog::Kind::Added:    return QColor(0x4A, 0xDE, 0x80);
    case Changelog::Kind::Fixed:    return QColor(0x60, 0xA5, 0xFA);
    case Changelog::Kind::Improved: return QColor(0xC0, 0x84, 0xFC);
    case Changelog::Kind::Changed:  return QColor(0xFB, 0xBF, 0x24);
    }
    return QColor(0x94, 0x9B, 0xA8);
}

QString pillStyle(Changelog::Kind kind)
{
    const QColor c = kindColor(kind);
    return QStringLiteral(
               "QLabel { color: rgba(%1,%2,%3,235);"
               " background-color: rgba(%1,%2,%3,22);"
               " border: 1px solid rgba(%1,%2,%3,70);"
               " border-radius: 7px; padding: 1px 0; font-size: 11px; }")
        .arg(c.red())
        .arg(c.green())
        .arg(c.blue());
}

QLabel *makePill(Changelog::Kind kind, QWidget *parent)
{
    auto *pill = new QLabel(Changelog::kindName(kind), parent);
    pill->setAlignment(Qt::AlignCenter);
    pill->setFixedSize(44, 20);
    pill->setStyleSheet(pillStyle(kind));
    return pill;
}

// 一条更新说明：左侧类型胶囊 + 右侧文字
QWidget *makeChangeRow(const Changelog::Item &item, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 1, 0, 1);
    layout->setSpacing(10);

    auto *pill = makePill(item.kind, row);
    layout->addWidget(pill, 0, Qt::AlignTop);

    auto *text = new QLabel(item.text, row);
    text->setObjectName(QStringLiteral("changelogBody"));
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(text, 1);

    return row;
}

// TODO / 已知问题的一条：小圆点 + 文字
QWidget *makeBulletRow(const QString &text, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 1, 0, 1);
    layout->setSpacing(10);

    auto *dot = new QLabel(QStringLiteral("·"), row);
    dot->setObjectName(QStringLiteral("changelogBullet"));
    dot->setFixedWidth(12);
    dot->setAlignment(Qt::AlignCenter);
    layout->addWidget(dot, 0, Qt::AlignTop);

    auto *label = new QLabel(text, row);
    label->setObjectName(QStringLiteral("changelogBullet"));
    label->setWordWrap(true);
    layout->addWidget(label, 1);

    return row;
}

QFrame *makeSeparator(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background-color: #242A36; border: none;"));
    return line;
}

/** 折叠区标题行：整行可点，右侧画展开箭头 */
class SectionHeader : public QWidget
{
public:
    explicit SectionHeader(const QString &title, const QString &trailing, bool compact,
                           QWidget *parent = nullptr)
        : QWidget(parent)
        , m_title(title)
        , m_trailing(trailing)
        , m_compact(compact)
    {
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setFixedHeight(compact ? 32 : 38);
    }

    void setExpanded(bool expanded)
    {
        m_expanded = expanded;
        update();
    }

    std::function<void()> onClicked;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (m_hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(kHoverBg);
            painter.drawRoundedRect(QRectF(rect()).adjusted(0, 1, 0, -1), 10, 10);
        }

        QFont titleFont = font();
        titleFont.setPixelSize(m_compact ? 12 : 13);
        titleFont.setWeight(QFont::DemiBold);
        titleFont.setFamily(QStringLiteral("Microsoft YaHei UI"));
        painter.setFont(titleFont);
        painter.setPen(m_compact ? QColor(0xD5, 0xDC, 0xE8) : kTitle);

        const int left = m_compact ? 14 : 12;
        const int right = 34;
        painter.drawText(QRect(left, 0, width() - left - right, height()),
                         Qt::AlignLeft | Qt::AlignVCenter, m_title);

        if (!m_trailing.isEmpty()) {
            QFont trailingFont = font();
            trailingFont.setPixelSize(11);
            painter.setFont(trailingFont);
            painter.setPen(kDim);
            painter.drawText(QRect(left, 0, width() - left - right, height()),
                             Qt::AlignRight | Qt::AlignVCenter, m_trailing);
        }

        // 展开箭头：收起时朝右，展开时朝下
        const qreal cx = width() - 17;
        const qreal cy = height() / 2.0;
        QPainterPath arrow;
        if (m_expanded) {
            arrow.moveTo(cx - 4.0, cy - 1.8);
            arrow.lineTo(cx, cy + 2.4);
            arrow.lineTo(cx + 4.0, cy - 1.8);
        } else {
            arrow.moveTo(cx - 1.8, cy - 4.0);
            arrow.lineTo(cx + 2.4, cy);
            arrow.lineTo(cx - 1.8, cy + 4.0);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(m_hovered ? QColor(0xC7, 0xD2, 0xE4) : kDim, 1.6,
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(arrow);
    }

    void enterEvent(QEnterEvent *event) override
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            if (onClicked)
                onClicked();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    QString m_title;
    QString m_trailing;
    bool m_compact = false;
    bool m_hovered = false;
    bool m_expanded = false;
};

/** 可折叠区域：标题行 + 内容（高度做展开 / 收起动画） */
class CollapsibleSection : public QWidget
{
public:
    CollapsibleSection(const QString &title, const QString &trailing, bool compact,
                       QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        m_header = new SectionHeader(title, trailing, compact, this);
        m_header->onClicked = [this]() { setExpanded(!m_expanded); };
        root->addWidget(m_header);

        m_body = new QWidget(this);
        m_body->setObjectName(QStringLiteral("changelogSectionBody"));
        m_body->setMaximumHeight(0);
        m_body->setVisible(false);

        m_bodyLayout = new QVBoxLayout(m_body);
        m_bodyLayout->setContentsMargins(compact ? 14 : 12, 2, 6, 10);
        m_bodyLayout->setSpacing(6);
        root->addWidget(m_body);
    }

    QVBoxLayout *bodyLayout() const { return m_bodyLayout; }

    void setExpanded(bool expanded, bool animate = true)
    {
        if (m_expanded == expanded)
            return;
        m_expanded = expanded;
        m_header->setExpanded(expanded);

        if (m_anim) {
            m_anim->stop();
            m_anim->deleteLater();
            m_anim = nullptr;
        }

        if (!animate) {
            if (expanded) {
                m_body->setVisible(true);
                m_body->setMaximumHeight(QWIDGETSIZE_MAX);
            } else {
                m_body->setMaximumHeight(0);
                m_body->setVisible(false);
            }
            return;
        }

        if (expanded) {
            m_body->setVisible(true);
            const int target = qMax(1, naturalHeight());
            auto *anim = new QPropertyAnimation(m_body, "maximumHeight", this);
            anim->setDuration(190);
            anim->setStartValue(0);
            anim->setEndValue(target);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &QPropertyAnimation::finished, m_body,
                    [this]() { m_body->setMaximumHeight(QWIDGETSIZE_MAX); });
            anim->start();
            m_anim = anim;
        } else {
            const int from = m_body->height();
            auto *anim = new QPropertyAnimation(m_body, "maximumHeight", this);
            anim->setDuration(170);
            anim->setStartValue(from);
            anim->setEndValue(0);
            anim->setEasingCurve(QEasingCurve::InCubic);
            connect(anim, &QPropertyAnimation::finished, m_body,
                    [this]() { m_body->setVisible(false); });
            anim->start();
            m_anim = anim;
        }
    }

    bool isExpanded() const { return m_expanded; }

private:
    int naturalHeight() const
    {
        if (m_body->layout() && m_body->layout()->hasHeightForWidth())
            return m_body->heightForWidth(qMax(m_body->width(), 240));
        return m_body->sizeHint().height();
    }

    SectionHeader *m_header = nullptr;
    QWidget *m_body = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;
    QPropertyAnimation *m_anim = nullptr;
    bool m_expanded = false;
};

} // namespace

ChangelogDialog::ChangelogDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("changelogDialog"));
    setWindowTitle(QStringLiteral("更新日志"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowTitleHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    setModal(true);
    resize(700, 520);
    setMinimumSize(520, 400);

    const Changelog::Release &current = Changelog::current();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(26, 22, 26, 16);
    root->setSpacing(12);

    // 标题行：更新日志 … Ver.x.y.z + 日期
    auto *titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("更新日志"), this);
    title->setObjectName(QStringLiteral("changelogTitle"));
    titleRow->addWidget(title);
    titleRow->addStretch(1);

    auto *version = new QLabel(QStringLiteral("Ver.%1").arg(current.version), this);
    version->setObjectName(QStringLiteral("changelogVersion"));
    titleRow->addWidget(version, 0, Qt::AlignVCenter);

    if (!current.date.isEmpty()) {
        auto *date = new QLabel(current.date, this);
        date->setObjectName(QStringLiteral("changelogDate"));
        titleRow->addWidget(date, 0, Qt::AlignVCenter);
    }
    root->addLayout(titleRow);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("changelogScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(scroll, 1);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("changelogContent"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(8);

    // 当前版本
    for (const Changelog::Item &item : current.items)
        layout->addWidget(makeChangeRow(item, content));

    layout->addSpacing(6);
    layout->addWidget(makeSeparator(content));
    layout->addSpacing(2);

    // 历史更新日志
    auto *history = new CollapsibleSection(QStringLiteral("历史更新日志"), QString(), false, content);
    history->setObjectName(QStringLiteral("changelogHistorySection"));
    {
        const QVector<Changelog::Release> &all = Changelog::releases();
        for (int i = 1; i < all.size(); ++i) {
            const Changelog::Release &release = all.at(i);
            auto *block = new CollapsibleSection(QStringLiteral("v%1").arg(release.version),
                                                 release.date, true, content);
            block->setObjectName(QStringLiteral("changelogRelease_") + release.version);
            for (const Changelog::Item &item : release.items)
                block->bodyLayout()->addWidget(makeChangeRow(item, block));
            history->bodyLayout()->addWidget(block);
        }
    }
    layout->addWidget(history);

    layout->addSpacing(2);
    layout->addWidget(makeSeparator(content));
    layout->addSpacing(2);

    // TODO / 已知问题
    auto *todo = new CollapsibleSection(QStringLiteral("TODO / 已知问题"), QString(), false, content);
    todo->setObjectName(QStringLiteral("changelogTodoSection"));
    for (const QString &line : Changelog::todo())
        todo->bodyLayout()->addWidget(makeBulletRow(line, todo));
    layout->addWidget(todo);

    layout->addStretch(1);
    scroll->setWidget(content);

    // 底部
    auto *footer = new QHBoxLayout();
    footer->addStretch(1);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setObjectName(QStringLiteral("dialogSecondary"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(closeBtn);
    root->addLayout(footer);
}

void ChangelogDialog::showChangelog(QWidget *parent)
{
    ChangelogDialog dialog(parent ? parent->window() : nullptr);
    dialog.exec();
}
