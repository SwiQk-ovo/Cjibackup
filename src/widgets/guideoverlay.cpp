#include "widgets/guideoverlay.h"
#include "core/settingsmanager.h"
#include "widgets/squircle.h"

#include <QGraphicsOpacityEffect>
#include <QGraphicsEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>

#include <QVBoxLayout>

namespace {

const QColor kMask(7, 10, 17, 196);
const QColor kAccent(59, 130, 246);

// 把目标控件所在的滚动区域滚到能看见它的位置
void scrollIntoView(QWidget *target)
{
    QWidget *parent = target->parentWidget();
    while (parent) {
        if (auto *scroll = qobject_cast<QScrollArea *>(parent)) {
            scroll->ensureWidgetVisible(target, 0, 56);
            return;
        }
        parent = parent->parentWidget();
    }
}

} // namespace

GuideOverlay::GuideOverlay(QWidget *host)
    : QWidget(host)
    , m_host(host)
{
    setObjectName(QStringLiteral("guideOverlay"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    hide();

    m_card = new QWidget(this);
    m_card->setObjectName(QStringLiteral("guideCard"));
    m_card->setAttribute(Qt::WA_StyledBackground, true);
    m_card->setFixedWidth(384);

    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(20, 16, 20, 16);
    cardLayout->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_stepLabel = new QLabel(m_card);
    m_stepLabel->setObjectName(QStringLiteral("guideStep"));
    topRow->addWidget(m_stepLabel);
    topRow->addStretch(1);

    m_skipBtn = new QPushButton(QStringLiteral("跳过引导"), m_card);
    m_skipBtn->setObjectName(QStringLiteral("guideSkip"));
    m_skipBtn->setCursor(Qt::PointingHandCursor);
    connect(m_skipBtn, &QPushButton::clicked, this, [this]() { finish(); });
    topRow->addWidget(m_skipBtn);
    cardLayout->addLayout(topRow);

    m_titleLabel = new QLabel(m_card);
    m_titleLabel->setObjectName(QStringLiteral("guideTitle"));
    m_titleLabel->setWordWrap(true);
    cardLayout->addWidget(m_titleLabel);

    m_bodyLabel = new QLabel(m_card);
    m_bodyLabel->setObjectName(QStringLiteral("guideBody"));
    m_bodyLabel->setWordWrap(true);
    cardLayout->addWidget(m_bodyLabel);

    cardLayout->addSpacing(6);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(10);
    buttonRow->addStretch(1);

    m_prevBtn = new QPushButton(QStringLiteral("上一步"), m_card);
    m_prevBtn->setObjectName(QStringLiteral("guidePrev"));
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
        if (m_index > 0)
            setStep(m_index - 1);
    });
    buttonRow->addWidget(m_prevBtn);

    m_nextBtn = new QPushButton(QStringLiteral("下一步"), m_card);
    m_nextBtn->setObjectName(QStringLiteral("guideNext"));
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setDefault(true);
    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        if (m_index + 1 >= m_steps.size())
            finish();
        else
            setStep(m_index + 1);
    });
    buttonRow->addWidget(m_nextBtn);

    cardLayout->addLayout(buttonRow);
}

void GuideOverlay::start(const QVector<Step> &steps)
{
    if (steps.isEmpty())
        return;

    m_steps = steps;
    m_index = 0;
    m_active = true;
    setGeometry(m_host->rect());
    raise();
    show();
    setFocus(Qt::OtherFocusReason);

    // 淡入：用数值动画控制遮罩透明度（不用 QGraphicsOpacityEffect，
    // 效果对象归 widget 所有，手动 delete 容易二次释放直接崩）
    m_fade = 0.0;
    if (!m_fadeAnim) {
        m_fadeAnim = new QVariantAnimation(this);
        m_fadeAnim->setDuration(180);
        m_fadeAnim->setStartValue(0.0);
        m_fadeAnim->setEndValue(1.0);
        m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_fadeAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_fade = v.toReal();
            update();
        });
    }
    m_fadeAnim->stop();
    m_fadeAnim->start();

    setStep(0);
}

void GuideOverlay::stop()
{
    finish();
}

void GuideOverlay::finish()
{
    if (!m_active)
        return;
    m_active = false;
    ++m_pending;
    hide();
    emit finished();
}

void GuideOverlay::setStep(int index)
{
    if (m_steps.isEmpty())
        return;

    m_index = qBound(0, index, m_steps.size() - 1);
    const Step &step = m_steps.at(m_index);

    if (step.prepare)
        step.prepare();

    // 先滚到可见位置，等页面淡入结束再抓图
    if (step.target) {
        if (QWidget *target = step.target())
            scrollIntoView(target);
    }

    updateCard();

    ++m_pending;
    const int token = m_pending;
    m_holeRect = QRect();
    update();

    // 下一轮事件循环再量尺寸：prepare() 刚刚切过页面，布局需要一次刷新
    QTimer::singleShot(30, this, [this, token]() {
        if (!m_active || token != m_pending)
            return;
        computeHole();
        layoutCard();
        update();
    });
}

void GuideOverlay::computeHole()
{
    QWidget *target = (m_index < m_steps.size() && m_steps.at(m_index).target)
                          ? m_steps.at(m_index).target()
                          : nullptr;

    if (!target || !target->isVisible() || target->width() <= 0 || target->height() <= 0) {
        // 目标暂时不可见：保留上一次的高亮，别让它消失一帧
        return;
    }


    // 直接在真实控件上开洞，不截屏、不缩放，所以不会糊
    const QPoint origin = target->mapTo(m_host, QPoint(0, 0));
    m_holeRect = QRect(origin, target->size()).adjusted(-6, -6, 6, 6);
    m_holeRect = m_holeRect.intersected(rect());
}
void GuideOverlay::updateCard()
{
    if (m_index >= m_steps.size())
        return;

    const Step &step = m_steps.at(m_index);
    m_stepLabel->setText(QStringLiteral("第 %1 步，共 %2 步")
                             .arg(m_index + 1)
                             .arg(m_steps.size()));
    m_titleLabel->setText(step.title);
    m_bodyLabel->setText(step.text);

    m_prevBtn->setEnabled(m_index > 0);
    m_nextBtn->setText(m_index + 1 >= m_steps.size() ? QStringLiteral("开始使用")
                                                     : QStringLiteral("下一步"));

    m_card->setFixedWidth(qMin(384, qMax(260, width() - 2 * 36)));
    m_card->adjustSize();
    layoutCard();
}

void GuideOverlay::layoutCard()
{
    if (!m_card)
        return;

    m_card->adjustSize();
    const int cardW = m_card->width();
    const int cardH = m_card->height();
    const int margin = 18;

    int x = margin;
    int y = margin;

    if (!m_holeRect.isEmpty()) {
        x = m_holeRect.left();
        y = m_holeRect.bottom() + 16;

        if (y + cardH + margin > height())
            y = m_holeRect.top() - 16 - cardH;
        if (y < margin)
            y = qBound(margin, m_holeRect.center().y() - cardH / 2, qMax(margin, height() - cardH - margin));
    } else {
        x = (width() - cardW) / 2;
        y = (height() - cardH) / 2;
    }

    x = qBound(margin, x, qMax(margin, width() - cardW - margin));
    y = qBound(margin, y, qMax(margin, height() - cardH - margin));
    m_card->move(x, y);
}

void GuideOverlay::relayout()
{
    setGeometry(m_host->rect());
    layoutCard();
    update();
}

void GuideOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutCard();
}

void GuideOverlay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor mask = kMask;
    mask.setAlphaF(qBound<qreal>(0.0, m_fade, 1.0) * 200.0 / 255.0);

    if (m_holeRect.isEmpty()) {
        painter.fillRect(rect(), mask);
        return;
    }

    const QRectF hole = QRectF(m_holeRect);

    // 遮罩直接"挖掉"高亮区域：下面就是真实控件，不经过任何截图/缩放
    QPainterPath maskPath;
    maskPath.addRect(QRectF(rect()));
    maskPath = maskPath.subtracted(Squircle::path(hole, 12));
    painter.fillPath(maskPath, mask);

    // 高亮圈外面的柔光
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(59, 130, 246, 55), 7.0));
    painter.drawPath(Squircle::path(hole.adjusted(-3, -3, 3, 3), 15));

    // 贴着控件画一圈描边
    painter.setPen(QPen(kAccent, 2.0));
    painter.drawPath(Squircle::path(hole, 12));
}

void GuideOverlay::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        finish();
        return;
    case Qt::Key_Right:
    case Qt::Key_Space:
        if (m_index + 1 >= m_steps.size())
            finish();
        else
            setStep(m_index + 1);
        return;
    case Qt::Key_Left:
        if (m_index > 0)
            setStep(m_index - 1);
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void GuideOverlay::mousePressEvent(QMouseEvent *event)
{
    // 遮罩吃掉点击，避免误触到下面的界面
    event->accept();
}
