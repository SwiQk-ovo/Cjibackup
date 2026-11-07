#include "settingsmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

// 默认备份目标：D: 只有真的存在且可写才用，否则落到图片目录下
// 老版本的 Cjibackup 目录如果还在，继续用它，免得照片分两处
QString defaultTargetPath()
{
    for (const QFileInfo &drive : QDir::drives()) {
        const QString root = drive.absoluteFilePath();
        if (!root.startsWith(QStringLiteral("D:"), Qt::CaseInsensitive) || !QFileInfo(root).isWritable())
            continue;
        const QString legacy = root + QStringLiteral("Cjibackup");
        if (QFileInfo::exists(legacy))
            return legacy;
        return root + QStringLiteral("KOPAL");
    }
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString legacy = QDir(pictures).filePath(QStringLiteral("Cjibackup"));
    if (QFileInfo::exists(legacy))
        return legacy;
    return QDir(pictures).filePath(QStringLiteral("KOPAL"));
}

} // namespace

SettingsManager* SettingsManager::instance()
{
    static SettingsManager inst;
    return &inst;
}

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent)
    , m_settings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
}

QVariant SettingsManager::value(const QString &key, const QVariant &def) const
{
    return m_settings.value(key, def);
}

void SettingsManager::store(const QString &key, const QVariant &v)
{
    m_settings.setValue(key, v);
    emit settingsChanged();
}

/* ---------------- 路径 ---------------- */

QString SettingsManager::sourcePath() const
{
    return value("sourcePath", QString()).toString();
}

void SettingsManager::setSourcePath(const QString &path)
{
    store("sourcePath", path);
    emit sourcePathChanged(path);
}

QString SettingsManager::targetPath() const
{
    return value("targetPath", defaultTargetPath()).toString();
}

void SettingsManager::setTargetPath(const QString &path)
{
    store("targetPath", path);
    emit targetPathChanged(path);
}

/* ---------------- 分类（旧版） ---------------- */

int SettingsManager::categoryMode() const
{
    return value("categoryMode", 1).toInt();
}

void SettingsManager::setCategoryMode(int mode)
{
    store("categoryMode", mode);
    emit categoryModeChanged(mode);
}

bool SettingsManager::createCameraFolder() const
{
    return value("createCameraFolder", false).toBool();
}

void SettingsManager::setCreateCameraFolder(bool v)
{
    store("createCameraFolder", v);
}

/* ---------------- 重命名 ---------------- */

bool SettingsManager::autoRename() const
{
    return value("autoRename", true).toBool();
}

void SettingsManager::setAutoRename(bool v)
{
    store("autoRename", v);
    emit autoRenameChanged(v);
}

QString SettingsManager::renameFormat() const
{
    return value("renameFormat", QStringLiteral("{brand}-{date}-{seq}")).toString();
}

void SettingsManager::setRenameFormat(const QString &fmt)
{
    store("renameFormat", fmt);
}

// v1.1.0：前缀 + 补零序号 + 自定义内容（备份页的三个胶囊）
QString SettingsManager::renamePrefix() const
{
    return value("renamePrefix", QStringLiteral("IMG")).toString();
}

void SettingsManager::setRenamePrefix(const QString &v)
{
    store("renamePrefix", v);
}

int SettingsManager::renameDigits() const
{
    return qBound(1, value("renameDigits", 4).toInt(), 8);
}

void SettingsManager::setRenameDigits(int v)
{
    store("renameDigits", qBound(1, v, 8));
}

QString SettingsManager::renameCustom() const
{
    return value("renameCustom", QString()).toString();
}

void SettingsManager::setRenameCustom(const QString &v)
{
    store("renameCustom", v);
}

/* ---------------- 子文件夹（旧版） ---------------- */

bool SettingsManager::subfolderByExt() const
{
    return value("subfolderByExt", false).toBool();
}

void SettingsManager::setSubfolderByExt(bool v)
{
    store("subfolderByExt", v);
}

/* ---------------- 校验 / 格式化 ---------------- */

bool SettingsManager::formatAfterBackup() const
{
    return value("formatAfterBackup", false).toBool();
}

void SettingsManager::setFormatAfterBackup(bool v)
{
    store("formatAfterBackup", v);
}

bool SettingsManager::verifyAfterBackup() const
{
    return value("verifyAfterBackup", true).toBool();
}

void SettingsManager::setVerifyAfterBackup(bool v)
{
    store("verifyAfterBackup", v);
}

bool SettingsManager::openFormatDialog() const
{
    return value("openFormatDialog", true).toBool();
}

void SettingsManager::setOpenFormatDialog(bool v)
{
    store("openFormatDialog", v);
}

/* ---------------- 多线程 ---------------- */

bool SettingsManager::multithread() const
{
    return value("multithread", true).toBool();
}

void SettingsManager::setMultithread(bool v)
{
    store("multithread", v);
    emit multithreadChanged(v);
}

int SettingsManager::threadCount() const
{
    return value("threadCount", 0).toInt();
}

void SettingsManager::setThreadCount(int n)
{
    store("threadCount", n);
}

/* ---------------- 退出 / 托盘 ---------------- */

int SettingsManager::exitBehavior() const
{
    return value("exitBehavior", 0).toInt();
}

void SettingsManager::setExitBehavior(int v)
{
    store("exitBehavior", v);
}

bool SettingsManager::minimizeToTray() const
{
    return exitBehavior() == 0;
}

bool SettingsManager::notifyOnComplete() const
{
    return value("notifyOnComplete", true).toBool();
}

void SettingsManager::setNotifyOnComplete(bool v)
{
    store("notifyOnComplete", v);
}

bool SettingsManager::startMinimized() const
{
    return value("startMinimized", false).toBool();
}

void SettingsManager::setStartMinimized(bool v)
{
    store("startMinimized", v);
}

/* ---------------- 动画倍速 ---------------- */

double SettingsManager::animationSpeed() const
{
    return value("animationSpeed", 1.0).toDouble();
}

void SettingsManager::setAnimationSpeed(double v)
{
    store("animationSpeed", v);
}

/* ---------------- 启动动画 / 音效 ---------------- */

bool SettingsManager::splashEnabled() const
{
    return value("splashEnabled", true).toBool();
}

void SettingsManager::setSplashEnabled(bool v)
{
    store("splashEnabled", v);
}

bool SettingsManager::fastStart() const
{
    return value("fastStart", false).toBool();
}

void SettingsManager::setFastStart(bool v)
{
    store("fastStart", v);
}

QString SettingsManager::splashPreset() const
{
    return value("splashPreset", QStringLiteral("swirl")).toString();
}

void SettingsManager::setSplashPreset(const QString &v)
{
    store("splashPreset", v);
}

QString SettingsManager::splashMediaPath() const
{
    return value("splashMediaPath", QString()).toString();
}

void SettingsManager::setSplashMediaPath(const QString &v)
{
    store("splashMediaPath", v);
}

QString SettingsManager::splashSoundPath() const
{
    return value("splashSoundPath", QString()).toString();
}

void SettingsManager::setSplashSoundPath(const QString &v)
{
    store("splashSoundPath", v);
}

QString SettingsManager::splashSoundMode() const
{
    return value("splashSoundMode", QStringLiteral("after")).toString();
}

void SettingsManager::setSplashSoundMode(const QString &v)
{
    store("splashSoundMode", v);
}

int SettingsManager::splashDurationMs() const
{
    return qBound(1000, value("splashDurationMs", 2600).toInt(), 10000);
}

void SettingsManager::setSplashDurationMs(int ms)
{
    store("splashDurationMs", qBound(1000, ms, 10000));
}

/* ---------------- 定时提醒 ---------------- */

bool SettingsManager::reminderEnabled() const
{
    return value("reminderEnabled", false).toBool();
}

void SettingsManager::setReminderEnabled(bool v)
{
    store("reminderEnabled", v);
}

QString SettingsManager::reminderMode() const
{
    return value("reminderMode", QStringLiteral("daily")).toString();
}

void SettingsManager::setReminderMode(const QString &v)
{
    store("reminderMode", v);
}

QString SettingsManager::reminderTime() const
{
    return value("reminderTime", QStringLiteral("20:00")).toString();
}

void SettingsManager::setReminderTime(const QString &v)
{
    store("reminderTime", v);
}

int SettingsManager::reminderWeekday() const
{
    return value("reminderWeekday", 1).toInt();
}

void SettingsManager::setReminderWeekday(int v)
{
    store("reminderWeekday", v);
}

QString SettingsManager::reminderDate() const
{
    return value("reminderDate", QString()).toString();
}

void SettingsManager::setReminderDate(const QString &v)
{
    store("reminderDate", v);
}

QString SettingsManager::reminderLastFired() const
{
    return value("reminderLastFired", QString()).toString();
}

void SettingsManager::setReminderLastFired(const QString &v)
{
    store("reminderLastFired", v);
}

/* ---------------- 自动更新 ---------------- */

bool SettingsManager::autoCheckUpdates() const
{
    return value("autoCheckUpdates", true).toBool();
}

void SettingsManager::setAutoCheckUpdates(bool v)
{
    store("autoCheckUpdates", v);
}

QString SettingsManager::updateChannel() const
{
    return value("updateChannel", QStringLiteral("release")).toString();
}

void SettingsManager::setUpdateChannel(const QString &v)
{
    store("updateChannel", v);
}

QString SettingsManager::notifiedVersion() const
{
    return value("notifiedVersion", QString()).toString();
}

void SettingsManager::setNotifiedVersion(const QString &v)
{
    store("notifiedVersion", v);
}

QString SettingsManager::skippedVersion() const
{
    return value("skippedVersion", QString()).toString();
}

void SettingsManager::setSkippedVersion(const QString &v)
{
    store("skippedVersion", v);
}

QString SettingsManager::lastUpdateCheck() const
{
    return value("lastUpdateCheck", QString()).toString();
}

void SettingsManager::setLastUpdateCheck(const QString &v)
{
    store("lastUpdateCheck", v);
}

/* ---------------- 流程图 ---------------- */

QStringList SettingsManager::flowConfig() const
{
    // 默认：年 / 月 / 日 / 后缀，等价于旧版的"按日分类 + 按后缀分子文件夹"
    return value("flowConfig",
                 QStringList() << "year" << "month" << "day" << "ext")
        .toStringList();
}

void SettingsManager::setFlowConfig(const QStringList &config)
{
    store("flowConfig", config);
}

/* ---------------- 使用引导 ---------------- */

bool SettingsManager::guideShown() const
{
    return value("guideShown", false).toBool();
}

void SettingsManager::setGuideShown(bool v)
{
    store("guideShown", v);
}

/* ---------------- 事件 ---------------- */

QStringList SettingsManager::events() const
{
    return value("events", QStringList()).toStringList();
}

void SettingsManager::setEvents(const QStringList &events)
{
    store("events", events);
    emit eventsChanged();
}

void SettingsManager::addEvent(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    QStringList list = events();
    if (list.contains(trimmed))
        return;
    list << trimmed;
    setEvents(list);
}

void SettingsManager::removeEvent(const QString &name)
{
    QStringList list = events();
    if (list.removeAll(name) > 0)
        setEvents(list);
}
