#ifndef PERFPROFILE_H
#define PERFPROFILE_H

#include <QByteArray>
#include <QString>

/**
 * 运行时硬件探测 + 自动优化档位。
 *
 * 全程后台执行，不弹窗、不写日志到界面。用户不会感知到"检测"这件事，
 * 只会觉得软件在他机器上跑得挺顺。
 */
class PerfProfile
{
public:
    enum Tier {
        Lite,        // 老机器 / 核显 / 内存小：牺牲画质点换流畅
        Balanced,    // 主流轻薄本、独显入门
        High         // 高性能机器：全开
    };

    static PerfProfile &instance();

    Tier tier() const { return m_tier; }
    int cores() const { return m_cores; }
    int ramMb() const { return m_ramMb; }
    QString gpuName() const { return m_gpu; }
    bool hasDiscreteGpu() const { return m_discrete; }

    // —— 给各模块用的建议值 ——
    int thumbProbeSlots() const;     // 同时探测几个视频首帧
    int thumbSide() const;           // 缩略图长边像素
    bool albumAnimations() const;    // 是否播放逐个淡入动画

    QByteArray mpvHwdec() const;     // 硬解策略
    QByteArray mpvCacheSecs() const;
    QByteArray mpvDemuxerMax() const;
    QByteArray mpvVo() const;
    int mpvThreads() const;

    QString tierName() const;

private:
    PerfProfile();
    void detect();

    Tier m_tier = Balanced;
    int m_cores = 4;
    int m_ramMb = 8192;
    QString m_gpu;
    bool m_discrete = false;
};

#endif // PERFPROFILE_H
