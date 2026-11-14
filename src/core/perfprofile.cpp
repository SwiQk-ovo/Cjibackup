#include "core/perfprofile.h"

#include <QSettings>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

int detectRamMb()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        return int(status.ullTotalPhys / (1024ull * 1024ull));
#endif
    return 8192;
}

} // namespace

PerfProfile &PerfProfile::instance()
{
    static PerfProfile profile;
    return profile;
}

PerfProfile::PerfProfile()
{
    detect();
}

void PerfProfile::detect()
{
    m_cores = qMax(1, QThread::idealThreadCount());
    m_ramMb = detectRamMb();

    // 显卡名字从注册表读，避免起 WMI 进程拖慢启动
    QSettings video(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\"
                       "{4d36e968-e325-11ce-bfc1-08002be10318}\\0000"),
        QSettings::NativeFormat);
    m_gpu = video.value(QStringLiteral("DriverDesc")).toString();
    if (m_gpu.isEmpty()) {
        QSettings video1(
            QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Class\\"
                           "{4d36e968-e325-11ce-bfc1-08002be10318}\\0001"),
            QSettings::NativeFormat);
        m_gpu = video1.value(QStringLiteral("DriverDesc")).toString();
    }

    const QString lower = m_gpu.toLower();
    const bool integrated = lower.contains(QStringLiteral("intel"))
                            && (lower.contains(QStringLiteral("hd"))
                                || lower.contains(QStringLiteral("uhd"))
                                || lower.contains(QStringLiteral("iris"))
                                || lower.contains(QStringLiteral("graphics")));
    m_discrete = !integrated && !m_gpu.isEmpty()
                 && !lower.contains(QStringLiteral("microsoft basic"));

    // 打分：核显小内存 → Lite；高端独显大内存多核 → High
    int score = 0;
    score += m_cores >= 12 ? 3 : (m_cores >= 8 ? 2 : (m_cores >= 6 ? 1 : 0));
    score += m_ramMb >= 32000 ? 3 : (m_ramMb >= 16000 ? 2 : (m_ramMb >= 8000 ? 1 : 0));
    score += m_discrete ? 2 : 0;

    if (score >= 6)
        m_tier = High;
    else if (score >= 3)
        m_tier = Balanced;
    else
        m_tier = Lite;

    // 核显 + 内存不足直接降档，避免 4K 视频卡成 PPT
    if (integrated && m_ramMb <= 8192)
        m_tier = Lite;
}

QString PerfProfile::tierName() const
{
    switch (m_tier) {
    case Lite: return QStringLiteral("lite");
    case High: return QStringLiteral("high");
    default: return QStringLiteral("balanced");
    }
}

int PerfProfile::thumbProbeSlots() const
{
    switch (m_tier) {
    case Lite: return 1;
    case High: return 4;
    default: return 2;
    }
}

int PerfProfile::thumbSide() const
{
    switch (m_tier) {
    case Lite: return 320;
    case High: return 480;
    default: return 400;
    }
}

bool PerfProfile::albumAnimations() const
{
    return m_tier != Lite;
}

QByteArray PerfProfile::mpvHwdec() const
{
    switch (m_tier) {
    case Lite: return QByteArrayLiteral("auto-safe");   // 老机仍尝试硬解，失败自动软解
    case High: return QByteArrayLiteral("auto");
    default: return QByteArrayLiteral("auto-safe");
    }
}

QByteArray PerfProfile::mpvCacheSecs() const
{
    switch (m_tier) {
    case Lite: return QByteArrayLiteral("1");
    case High: return QByteArrayLiteral("6");
    default: return QByteArrayLiteral("2");
    }
}

QByteArray PerfProfile::mpvDemuxerMax() const
{
    switch (m_tier) {
    case Lite: return QByteArrayLiteral("24M");
    case High: return QByteArrayLiteral("256M");
    default: return QByteArrayLiteral("50M");
    }
}

QByteArray PerfProfile::mpvVo() const
{
    switch (m_tier) {
    case Lite: return QByteArrayLiteral("gpu");
    case High: return QByteArrayLiteral("gpu-next");
    default: return QByteArrayLiteral("gpu");
    }
}

int PerfProfile::mpvThreads() const
{
    switch (m_tier) {
    case Lite: return 2;
    case High: return qMax(4, m_cores / 2);
    default: return qMax(2, m_cores / 3);
    }
}
