#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

class SettingsManager : public QObject
{
    Q_OBJECT

public:
    static SettingsManager* instance();

    // 路径设置
    QString sourcePath() const;
    void setSourcePath(const QString &path);
    QString targetPath() const;
    void setTargetPath(const QString &path);

    // 分类设置（旧版兼容）
    int categoryMode() const; // 0=按日, 1=按月, 2=按年
    void setCategoryMode(int mode);
    bool createCameraFolder() const;
    void setCreateCameraFolder(bool v);

    // 重命名设置
    bool autoRename() const;
    void setAutoRename(bool v);
    QString renameFormat() const;
    void setRenameFormat(const QString &fmt);

    // 文件重命名（v1.1.0：前缀 + 补零序号 + 自定义内容）
    QString renamePrefix() const;         // 默认 IMG
    void setRenamePrefix(const QString &v);
    int renameDigits() const;             // 默认 4，范围 1~8
    void setRenameDigits(int v);
    QString renameCustom() const;         // 默认空
    void setRenameCustom(const QString &v);

    // 子文件夹（按后缀，旧版兼容）
    bool subfolderByExt() const;
    void setSubfolderByExt(bool v);

    // 格式化 / 校验
    bool formatAfterBackup() const;
    void setFormatAfterBackup(bool v);
    QString pathSeparator() const;
    void setPathSeparator(const QString &sep);
    // 拖节点合并时，弹窗里上次选的分隔符（首次记住）
    QString mergeSeparator() const;
    void setMergeSeparator(const QString &sep);

    bool verifyAfterBackup() const;
    void setVerifyAfterBackup(bool v);
    bool openFormatDialog() const;
    void setOpenFormatDialog(bool v);

    // 多线程
    bool multithread() const;
    void setMultithread(bool v);
    int threadCount() const;              // 0 = 自动
    void setThreadCount(int n);

    // 退出 / 托盘
    int exitBehavior() const;             // 0=最小化到托盘, 1=退出
    void setExitBehavior(int v);
    bool minimizeToTray() const;
    bool notifyOnComplete() const;
    void setNotifyOnComplete(bool v);
    bool startMinimized() const;
    void setStartMinimized(bool v);

    // 动画倍速
    double animationSpeed() const;
    void setAnimationSpeed(double v);

    // 启动动画 / 音效
    bool splashEnabled() const;
    void setSplashEnabled(bool v);
    bool fastStart() const;
    void setFastStart(bool v);
    QString splashPreset() const;         // swirl / fade / flash / custom
    void setSplashPreset(const QString &v);
    QString splashMediaPath() const;
    void setSplashMediaPath(const QString &v);
    QString splashSoundPath() const;
    void setSplashSoundPath(const QString &v);
    QString splashSoundMode() const;      // after / smooth / random
    void setSplashSoundMode(const QString &v);
    int splashDurationMs() const;         // 1000~10000
    void setSplashDurationMs(int ms);

    // 定时提醒
    bool reminderEnabled() const;
    void setReminderEnabled(bool v);
    QString reminderMode() const;         // daily/weekly/monthly/yearly/once
    void setReminderMode(const QString &v);
    QString reminderTime() const;         // "HH:mm"
    void setReminderTime(const QString &v);
    int reminderWeekday() const;          // 1=周一 ... 7=周日
    void setReminderWeekday(int v);
    QString reminderDate() const;         // yyyy-MM-dd（仅一次）
    void setReminderDate(const QString &v);
    QString reminderLastFired() const;    // ISO 时间
    void setReminderLastFired(const QString &v);

    // 自动更新
    bool autoCheckUpdates() const;
    void setAutoCheckUpdates(bool v);
    QString updateChannel() const;        // test / release
    void setUpdateChannel(const QString &v);
    QString notifiedVersion() const;   // 已经提示过（浮层/托盘）的版本
    void setNotifiedVersion(const QString &v);
    QString skippedVersion() const;
    void setSkippedVersion(const QString &v);
    QString lastUpdateCheck() const;
    void setLastUpdateCheck(const QString &v);

    // 分类流程图配置
    QStringList flowConfig() const;
    void setFlowConfig(const QStringList &config);

    // 首次启动的使用引导是否看过
    bool guideShown() const;
    void setGuideShown(bool v);

    // 事件名称列表（事件页 / 流程图的"事件"节点共用）
    QStringList events() const;
    void setEvents(const QStringList &events);
    void addEvent(const QString &name);
    void removeEvent(const QString &name);

signals:
    void sourcePathChanged(const QString &path);
    void targetPathChanged(const QString &path);
    void categoryModeChanged(int mode);
    void autoRenameChanged(bool v);
    void multithreadChanged(bool v);
    void eventsChanged();
    void settingsChanged();

private:
    explicit SettingsManager(QObject *parent = nullptr);
    QVariant value(const QString &key, const QVariant &def) const;
    void store(const QString &key, const QVariant &v);

    QSettings m_settings;
};

#endif // SETTINGSMANAGER_H
