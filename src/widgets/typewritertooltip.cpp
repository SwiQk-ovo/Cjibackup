#include "typewritertooltip.h"

#include <QCursor>
#include <QEnterEvent>
#include <QFontMetrics>
#include <QApplication>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

namespace {
constexpr int kPaddingX = 14;
constexpr int kPaddingY = 10;
constexpr int kMaxTextWidth = 380;
constexpr int kTickMs = 22;
constexpr int kCharsPerTick = 1;
} // namespace

TypewriterTooltip *TypewriterTooltip::instance()
{
    static TypewriterTooltip *tip = new TypewriterTooltip();
    return tip;
}

TypewriterTooltip::TypewriterTooltip(QWidget *parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);

    QFont f = font();
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(13);
    setFont(f);

    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &TypewriterTooltip::tick);
}

void TypewriterTooltip::showText(const QString &text, QWidget *anchor)
{
    if (text.isEmpty())
        return;

    if (m_text == text && isVisible() && !m_deleting) {
        m_anchor = anchor;
        applyGeometry();
        return;
    }

    m_text = text;
    m_anchor = anchor;
    m_visibleChars = 0;
    m_deleting = false;
    m_idleTicks = 0;

    applyGeometry();
    show();
    raise();
    m_timer->start();
}

void TypewriterTooltip::dismiss()
{
    if (!isVisible())
        return;
    if (m_deleting)
        return;

    // 已打出的文字逐字删除后再隐藏
    m_deleting = true;
    m_idleTicks = 0;
    m_timer->start();
}

void TypewriterTooltip::applyGeometry()
{
    QWidget *reference = m_anchor ? m_anchor->window() : QApplication::activeWindow();

    const QFontMetrics fm(font());
    const QRect textRect = fm.boundingRect(QRect(0, 0, kMaxTextWidth, 400),
                                           Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                           m_text);

    const int width = qBound(140, textRect.width() + kPaddingX * 2 + 2, kMaxTextWidth + kPaddingX * 2 + 8);
    const int height = textRect.height() + kPaddingY * 2;
    resize(width, height);

    if (!reference) {
        move(QCursor::pos() - QPoint(width / 2, height + 26));
        return;
    }

    const QRect win = reference->frameGeometry();
    int x = win.center().x() - width / 2;
    int y = win.top() + 64;

    const QRect screen = (reference->windowHandle() && reference->windowHandle()->screen())
                             ? reference->windowHandle()->screen()->availableGeometry()
                             : QGuiApplication::primaryScreen()->availableGeometry();
    x = qBound(screen.left() + 8, x, screen.right() - width - 8);
    y = qBound(screen.top() + 8, y, screen.bottom() - height - 8);

    move(x, y);
}

void TypewriterTooltip::tick()
{
    if (m_deleting) {
        m_visibleChars -= kCharsPerTick * 2;
        if (m_visibleChars <= 0) {
            m_visibleChars = 0;
            m_timer->stop();
            hide();
            m_text.clear();
            update();
            return;
        }
        update();
        return;
    }

    if (m_visibleChars < m_text.size()) {
        m_visibleChars = qMin(m_visibleChars + kCharsPerTick, m_text.size());
        update();
        if (m_visibleChars == m_text.size())
            m_idleTicks = 0;
        return;
    }

    // 文字已经全部显示：不再自动删除。
    // 只有在鼠标确实离开锚点（偶尔会漏掉 leaveEvent）时才收起。
    if (m_anchor && !m_anchor->rect().contains(m_anchor->mapFromGlobal(QCursor::pos()))) {
        if (++m_idleTicks > 45)
            dismiss();
    } else {
        m_idleTicks = 0;
    }
}

void TypewriterTooltip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bubble = QRectF(rect()).adjusted(1, 1, -1, -1);
    painter.setPen(QPen(QColor(255, 255, 255, 26), 1));
    painter.setBrush(QColor(6, 9, 14, 224));
    painter.drawRoundedRect(bubble, 10, 10);

    // 顶部一条淡蓝色高光，和整体主题呼应
    QLinearGradient accent(bubble.topLeft(), QPointF(bubble.right(), bubble.top()));
    accent.setColorAt(0.0, QColor(59, 130, 246, 150));
    accent.setColorAt(1.0, QColor(59, 130, 246, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent);
    painter.drawRoundedRect(QRectF(bubble.left() + 8, bubble.top(), bubble.width() - 16, 2), 1, 1);

    painter.setPen(QColor(255, 255, 255));
    painter.drawText(bubble.adjusted(kPaddingX, kPaddingY, -kPaddingX, -kPaddingY),
                     Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                     m_text.left(m_visibleChars));
}

/* ------------------------------------------------------------------ */

HelpIcon::HelpIcon(const QString &text, QWidget *parent)
    : QLabel(parent)
    , m_text(text)
{
    setObjectName(QStringLiteral("helpIcon"));
    setFixedSize(16, 16);
    setAlignment(Qt::AlignCenter);
    setText(QStringLiteral("!"));
    setCursor(Qt::WhatsThisCursor);
    setAttribute(Qt::WA_Hover);
}

void HelpIcon::enterEvent(QEnterEvent *event)
{
    TypewriterTooltip::instance()->showText(m_text, this);
    QLabel::enterEvent(event);
}

void HelpIcon::leaveEvent(QEvent *event)
{
    TypewriterTooltip::instance()->dismiss();
    QLabel::leaveEvent(event);
}
