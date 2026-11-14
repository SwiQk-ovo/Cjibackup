#include "core/perfmonitor.h"

#include <QDateTime>

namespace {
constexpr int kWallBudgetMs = 12;    // 一帧超过 12ms（<83fps）算吃力
constexpr int kWallSamples = 8;      // 连续 8 帧吃力就降级
constexpr int kVideoBudgetMs = 48;   // 帧间隔 > 48ms（<21fps）算掉帧
constexpr int kVideoSamples = 20;
} // namespace

PerfMonitor &PerfMonitor::instance()
{
    static PerfMonitor monitor;
    return monitor;
}

PerfMonitor::PerfMonitor(QObject *parent)
    : QObject(parent)
{
}

void PerfMonitor::reportWallFrame(int ms)
{
    if (m_wallDegraded || ms <= 0)
        return;
    if (ms > kWallBudgetMs) {
        if (++m_wallSlow >= kWallSamples) {
            m_wallDegraded = true;
            m_wallSlow = 0;
            emit wallDegradedChanged();
        }
    } else {
        m_wallSlow = qMax(0, m_wallSlow - 1);
    }
}

void PerfMonitor::reportVideoFrame(int intervalMs)
{
    if (m_videoDegraded || intervalMs <= 0)
        return;
    if (intervalMs > kVideoBudgetMs) {
        if (++m_videoSlow >= kVideoSamples) {
            m_videoDegraded = true;
            m_videoFastScale = true;   // 先降缩放质量
            emit videoDegradedChanged();
        }
    } else {
        m_videoSlow = qMax(0, m_videoSlow - 1);
    }
}
