#ifndef PERFMONITOR_H
#define PERFMONITOR_H

#include <QElapsedTimer>
#include <QObject>

/**
 * 第二层动态调整：不看硬件参数，看**实际跑起来的表现**。
 *
 * 采集两类真实开销：
 *   1) 素材墙每帧绘制耗时
 *   2) 播放器每帧上屏间隔
 *
 * 连续多次不达标就静默降一级画质（不回弹，避免抖动）。
 * 用户完全感知不到"检测/降级"这件事，只会觉得界面变顺了。
 */
class PerfMonitor : public QObject
{
    Q_OBJECT

public:
    static PerfMonitor &instance();

    // 素材墙：每帧绘制耗时（毫秒）
    void reportWallFrame(int ms);
    // 播放器：两帧之间的间隔（毫秒），0 表示非法值
    void reportVideoFrame(int intervalMs);

    bool wallDegraded() const { return m_wallDegraded; }
    bool videoDegraded() const { return m_videoDegraded; }
    bool videoFastScale() const { return m_videoFastScale; }

signals:
    void wallDegradedChanged();
    void videoDegradedChanged();

private:
    explicit PerfMonitor(QObject *parent = nullptr);

    int m_wallSlow = 0;
    int m_videoSlow = 0;
    bool m_wallDegraded = false;
    bool m_videoDegraded = false;
    bool m_videoFastScale = false;
    qint64 m_lastVideoFrame = 0;
};

#endif // PERFMONITOR_H
