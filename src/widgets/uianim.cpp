#include "uianim.h"

#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

namespace UiAnim {

void fadeIn(QWidget *widget, int durationMs, int delayMs)
{
    if (!widget)
        return;

    auto *effect = qobject_cast<QGraphicsOpacityEffect *>(widget->graphicsEffect());
    if (!effect) {
        effect = new QGraphicsOpacityEffect(widget);
        effect->setOpacity(1.0);
        widget->setGraphicsEffect(effect);
    }

    auto *anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(durationMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);

    if (delayMs > 0) {
        QPointer<QPropertyAnimation> guard(anim);
        QTimer::singleShot(delayMs, widget, [guard]() {
            if (guard)
                guard->start();
        });
    } else {
        anim->start();
    }
}

void staggerCards(QWidget *page, int durationMs, int stepMs)
{
    if (!page)
        return;

    int index = 0;
    const QList<QFrame *> frames = page->findChildren<QFrame *>(QString(), Qt::FindChildrenRecursively);
    for (QFrame *frame : frames) {
        if (frame->frameShape() != QFrame::NoFrame)
            continue;
        // 只给"卡片"类容器加动画，避免把输入框、分隔线也算进来
        if (!frame->objectName().isEmpty())
            continue;
        if (frame->height() < 60)
            continue;

        fadeIn(frame, durationMs, index * stepMs);
        ++index;
        if (index >= 8)
            break;
    }
}

void pageIn(QWidget *page, int durationMs)
{
    if (!page)
        return;

    fadeIn(page, durationMs, 0);

    // 页面内容再稍微晚一点淡入，形成一点层次
    if (QWidget *content = page->findChild<QWidget *>(QStringLiteral("pageContent"))) {
        if (content->height() > 120)
            fadeIn(content, durationMs + 40, 20);
    }
}

} // namespace UiAnim
