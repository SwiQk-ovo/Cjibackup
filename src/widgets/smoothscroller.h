#ifndef SMOOTHSCROLLER_H
#define SMOOTHSCROLLER_H

#include <QApplication>
#include <QHash>
#include <QObject>

class QPropertyAnimation;
class QScrollBar;

/**
 * 全局滚轮平滑器：
 * - 在 QApplication::notify 里拦 QAbstractScrollArea 的滚轮事件，
 *   把滚动条从当前位置动画到目标位置
 * - 不碰下拉框 / 数字框 / 滑块，避免抢掉它们自己的滚轮行为
 * - 设置页、备份页、素材墙、更新窗口、更新日志都自动生效
 */
class SmoothScroller : public QObject
{
    Q_OBJECT

public:
    // 返回 true 表示事件已经被消费
    static bool handle(QObject *watched, QEvent *event);

private:
    explicit SmoothScroller(QObject *parent = nullptr);
    static SmoothScroller *instance();

    bool filter(QObject *watched, QEvent *event);

    QHash<QScrollBar *, QPropertyAnimation *> m_anims;
};

/** 用 notify 全局过滤滚轮；普通 QApplication 只能过滤它自己的事件 */
class KopalApplication : public QApplication
{
public:
    using QApplication::QApplication;

    bool notify(QObject *receiver, QEvent *event) override
    {
        if (SmoothScroller::handle(receiver, event))
            return true;
        return QApplication::notify(receiver, event);
    }
};

#endif // SMOOTHSCROLLER_H
