#ifndef UIANIM_H
#define UIANIM_H

class QWidget;

namespace UiAnim {

// 淡入（可选延迟），页面切换 / 卡片进场用
void fadeIn(QWidget *widget, int durationMs = 200, int delayMs = 0);

// 页面切换：整页淡入 + 内部卡片错峰淡入
void pageIn(QWidget *page, int durationMs = 170);

// 卡片错峰进场（只处理 QFrame 类型的卡片容器）
void staggerCards(QWidget *page, int durationMs = 220, int stepMs = 26);

} // namespace UiAnim

#endif // UIANIM_H
