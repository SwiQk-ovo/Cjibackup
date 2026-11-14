#include "widgets/squircle.h"
#include "toast.h"

#include <QEnterEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <QVBoxLayout>

namespace {

Toast *g_toast = nullptr;

constexpr int kWidth = 360;
constexpr int kMargin = 24;

QColor accentFor(int kind)
{
    switch (kind) {
    case Toast::Success:  return QColor(0x34, 0xD3, 0x99);
    case Toast::Warning:  return QColor(0xF5, 0x9E, 0x0B);
    case Toast::Error:    return QColor(0xF8, 0x71, 0x71);
    case Toast::Reminder: return QColor(0x60, 0xA5, 0xFA);
    default:              return QColor(0x4C, 0x8D, 0xFF);
    }
}

QString glyphFor(int kind)
{
    switch (kind) {
    case Toast::Success:  return QStringLiteral("✓");
    case Toast::Warning:  return QStringLiteral("!");
    case Toast::Error:    return QStringLiteral("×");
    case Toast::Reminder: return QStringLiteral("⏰");
    default:              return QStringLiteral("i");
    }
}

QPixmap iconPixmap(int kind, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF circle(1, 1, size - 2, size - 2);
    QLinearGradient gradient(circle.topLeft(), circle.bottomRight());
    gradient.setColorAt(0.0, accentFor(kind).lighter(125));
    gradient.setColorAt(1.0, accentFor(kind));

    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(circle);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(int(size * 0.52));
    font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(circle, Qt::AlignCenter, glyphFor(kind));
    painter.end();
    return pixmap;
}

} // namespace

Toast::Toast()
    : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                          | Qt::WindowDoesNotAcceptFocus)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlag(Qt::WindowTransparentForInput, false);
    setFixedWidth(kWidth);
    setCursor(Qt::PointingHandCursor);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_card = new SquircleCard(this);
    m_card->setObjectName(QStringLiteral("toastCard"));
    outer->addWidget(m_card);

    auto *shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(0, 0, 0, 170));
    shadow->setOffset(0, 6);
    m_card->setGraphicsEffect(shadow);

    auto *layout = new QHBoxLayout(m_card);
    layout->setContentsMargins(16, 14, 14, 14);
    layout->setSpacing(12);

    m_icon = new QLabel(m_card);
    m_icon->setFixedSize(34, 34);
    layout->addWidget(m_icon, 0, Qt::AlignTop);

    auto *textBox = new QVBoxLayout();
    textBox->setSpacing(3);

    m_title = new QLabel(m_card);
    m_title->setObjectName(QStringLiteral("toastTitle"));
    textBox->addWidget(m_title);

    m_text = new QLabel(m_card);
    m_text->setObjectName(QStringLiteral("toastText"));
    m_text->setWordWrap(true);
    m_text->setTextFormat(Qt::PlainText);
    textBox->addWidget(m_text);

    m_action = new QPushButton(m_card);
    m_action->setObjectName(QStringLiteral("toastAction"));
    m_action->setCursor(Qt::PointingHandCursor);
    m_action->hide();
    textBox->addWidget(m_action, 0, Qt::AlignLeft);

    layout->addLayout(textBox, 1);

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &Toast::hideToast);

    m_slide = new QPropertyAnimation(this, "pos", this);
    m_slide->setDuration(260);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);

    m_fade = new QPropertyAnimation(this, "windowOpacity", this);
    m_fade->setDuration(220);
}

void Toast::applyContent(const QString &title, const QString &text, Kind kind,
                         const QString &actionText, std::function<void()> onAction)
{
    m_icon->setPixmap(iconPixmap(kind, 34));
    m_title->setText(title);
    m_text->setText(text);
    m_text->setVisible(!text.isEmpty());

    m_action->disconnect();
    m_action->setVisible(!actionText.isEmpty());
    if (!actionText.isEmpty()) {
        m_action->setText(actionText);
        connect(m_action, &QPushButton::clicked, this, [this, onAction]() {
            hideToast();
            if (onAction)
                onAction();
        });
    }
    m_onAction = onAction;

    adjustSize();
}

void Toast::reposition()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (QScreen *anchorScreen = windowHandle() ? windowHandle()->screen() : nullptr)
        screen = anchorScreen;

    const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    const int x = available.right() - width() - kMargin;
    const int y = available.bottom() - height() - kMargin;
    m_hiddenPos = QPoint(x, y);
}

void Toast::present(int durationMs)
{
    m_duration = durationMs;
    reposition();

    const QPoint target = m_hiddenPos;
    const QPoint start(target.x(), target.y() + 18);

    if (isVisible()) {
        move(target);
        setWindowOpacity(1.0);
    } else {
        setWindowOpacity(0.0);
        move(start);
        show();
    }
    raise();

    m_slide->stop();
    m_slide->setStartValue(pos());
    m_slide->setEndValue(target);
    m_slide->start();

    m_fade->stop();
    m_fade->setStartValue(windowOpacity());
    m_fade->setEndValue(1.0);
    m_fade->start();

    m_hideTimer->start(m_duration);
}

void Toast::hideToast()
{
    m_hideTimer->stop();

    m_fade->stop();
    m_fade->setStartValue(windowOpacity());
    m_fade->setEndValue(0.0);
    m_fade->disconnect();
    connect(m_fade, &QPropertyAnimation::finished, this, [this]() {
        hide();
        setWindowOpacity(1.0);
    });
    m_fade->start();
}

void Toast::enterEvent(QEnterEvent *event)
{
    m_hideTimer->stop();
    QWidget::enterEvent(event);
}

void Toast::leaveEvent(QEvent *event)
{
    if (isVisible())
        m_hideTimer->start(m_duration);
    QWidget::leaveEvent(event);
}

void Toast::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        hideToast();
        // 点卡片本身（不是按钮）也算"去看看"
        if (m_onAction) {
            const auto action = m_onAction;
            m_onAction = nullptr;
            action();
        }
        return;
    }
    QWidget::mousePressEvent(event);
}

void Toast::showToast(const QString &title, const QString &text, Kind kind, int durationMs,
                      const QString &actionText, std::function<void()> onAction)
{
    if (!g_toast)
        g_toast = new Toast();

    g_toast->applyContent(title, text, kind, actionText, std::move(onAction));
    g_toast->present(durationMs);
}

void Toast::dismissAll()
{
    if (g_toast)
        g_toast->hideToast();
}
