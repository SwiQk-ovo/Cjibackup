#include "smoothscroller.h"

#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QSlider>
#include <QWheelEvent>
#include <QWidget>

SmoothScroller::SmoothScroller(QObject *parent)
    : QObject(parent)
{
}

SmoothScroller *SmoothScroller::instance()
{
    static SmoothScroller *scroller = new SmoothScroller(qApp);
    return scroller;
}

bool SmoothScroller::handle(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::Wheel)
        return false;
    return instance()->filter(watched, event);
}

bool SmoothScroller::filter(QObject *watched, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(watched);
    if (!widget)
        return false;

    // 这些控件自己的滚轮有含义（切选项 / 调数值），别抢
    if (qobject_cast<QComboBox *>(widget) || qobject_cast<QAbstractSpinBox *>(widget)
        || qobject_cast<QSlider *>(widget))
        return false;

    auto *wheel = static_cast<QWheelEvent *>(event);
    if (wheel->modifiers() & Qt::ControlModifier)
        return false;

    QAbstractScrollArea *area = nullptr;
    for (QWidget *w = widget; w; w = w->parentWidget()) {
        if (auto *candidate = qobject_cast<QAbstractScrollArea *>(w)) {
            area = candidate;
            break;
        }
        // 滚轮落在下拉框/数字框内部（比如下拉列表）时不要接管
        if (qobject_cast<QComboBox *>(w) || qobject_cast<QAbstractSpinBox *>(w)
            || qobject_cast<QSlider *>(w))
            return false;
    }
    if (!area)
        return false;

    const QPoint angle = wheel->angleDelta();
    const QPoint pixel = wheel->pixelDelta();
    const bool horizontal = qAbs(angle.x()) > qAbs(angle.y())
                            || (angle.y() == 0 && qAbs(pixel.x()) > qAbs(pixel.y()));
    QScrollBar *bar = horizontal ? area->horizontalScrollBar() : area->verticalScrollBar();
    if (!bar || bar->maximum() <= bar->minimum())
        return false;

    int delta = horizontal ? (pixel.x() != 0 ? pixel.x() : angle.x())
                           : (pixel.y() != 0 ? pixel.y() : angle.y());
    if (delta == 0)
        return false;
    if (pixel.isNull())
        delta = int(delta / 120.0 * 90.0);   // 普通滚轮一格 ≈ 90px

    QPropertyAnimation *anim = m_anims.value(bar);
    const bool running = anim && anim->state() == QAbstractAnimation::Running;
    const int base = running ? anim->endValue().toInt() : bar->value();
    const int target = qBound(bar->minimum(), base - delta, bar->maximum());

    if (!anim) {
        anim = new QPropertyAnimation(bar, "value", this);
        anim->setDuration(240);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        m_anims.insert(bar, anim);
        connect(bar, &QObject::destroyed, this, [this, bar]() { m_anims.remove(bar); });
    }

    anim->stop();
    anim->setStartValue(bar->value());
    anim->setEndValue(target);
    anim->start();

    wheel->accept();
    return true;
}
