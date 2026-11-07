#ifndef BACKUPENGINE_H
#define BACKUPENGINE_H

#include <QDateTime>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

#include <atomic>

#include "core/cameranaming.h"

struct BackupStats {
    int totalFiles = 0;
    int copiedFiles = 0;
    int failedCount = 0;
    qint64 totalBytes = 0;
    qint64 copiedBytes = 0;
    double speedMBps = 0;
    QString currentFile;
};

struct VerifyResult {
    bool ran = false;
    int checked = 0;
    int missing = 0;
    QStringList missingFiles;
};

// 一个待备份文件的完整计划：源文件 → 目标相对目录 / 文件名。
// 在 collectFiles() 里一次性算好（单线程、结果可复现），
// 这样多线程拷贝、以及备份后的校验，拿到的都是同一份路径。
struct PlannedFile
{
    QFileInfo info;
    QString relativeDir;   // 相对目标根目录，可能为空
    QString fileName;      // 最终文件名（含后缀）
    QString dstPath;       // 绝对目标路径
    QString cameraModel;   // 读到的相机型号，读不到为空
};

class BackupWorker;

class BackupEngine : public QObject
{
    Q_OBJECT

public:
    enum State { Idle, Running, Paused, Completed, Stopped, Error };
    Q_ENUM(State)

    explicit BackupEngine(QObject *parent = nullptr);
    ~BackupEngine();

    void start(const QString &source, const QString &target);
    void pause();
    void resume();
    void stop();

    State state() const;
    BackupStats stats() const;
    QStringList failedFiles() const;
    VerifyResult verifyResult() const;

    int activeThreads() const { return m_threads; }
    QString targetPath() const { return m_target; }
    QString sourcePath() const { return m_source; }

    // 只应由后台线程调用：协调线程跑 runBackup，其余线程跑 copyLoop
    void runBackup();
    void copyLoop();

signals:
    void progressUpdated(const BackupStats &stats);
    void stateChanged(BackupEngine::State state);
    void fileCopied(const QString &src, const QString &dst);
    void errorOccurred(const QString &message);
    void verified(const VerifyResult &result);
    void completed();

private:
    void setState(State state);
    void collectFiles();
    bool copyOne(const PlannedFile &file);
    void verifyAll();
    QDateTime dateOf(const QFileInfo &info) const;
    QString cameraModelFor(const QFileInfo &info);
    QString relativeDirFor(const QFileInfo &info, const QString &cameraFolder) const;
    bool copyFile(const QString &src, const QString &dst);
    bool ensureDir(const QString &dir);
    void emitProgress(bool force);
    QList<QPointer<BackupWorker>> workersSnapshot();

    // 启动时在主线程做快照，后台线程不再访问 QSettings
    QString m_source;
    QString m_target;
    QStringList m_flowConfig;
    bool m_autoRename = false;
    NamingPlan m_naming;                 // 前缀 + 补零序号 + 自定义内容
    bool m_legacySubfolderByExt = false;
    int m_legacyCategoryMode = 1;
    bool m_verifyEnabled = true;
    int m_threads = 1;
    int m_totalFiles = 0;

    QVector<PlannedFile> m_plan;
    QHash<QString, QString> m_modelCache;   // 「文件所在目录 + 文件大小」→ 型号
    CameraFolderAllocator m_cameraFolders;  // 型号 → 目标文件夹名
    QStringList m_failedFiles;
    VerifyResult m_verify;

    State m_state = Idle;
    BackupStats m_stats;

    mutable QMutex m_mutex;
    QMutex m_dirMutex;                 // 串行化 mkpath
    QSet<QString> m_createdDirs;
    QWaitCondition m_pauseCond;
    bool m_paused = false;
    bool m_stopped = false;
    std::atomic<int> m_nextIndex{0};

    QElapsedTimer m_emitTimer;
    QElapsedTimer m_runTimer;

    QList<QPointer<BackupWorker>> m_workers;
};

// 后台拷贝线程
class BackupWorker : public QThread
{
    Q_OBJECT
public:
    explicit BackupWorker(BackupEngine *engine, bool coordinator, QObject *parent = nullptr);
protected:
    void run() override;
private:
    BackupEngine *m_engine;
    bool m_coordinator;
};

#endif // BACKUPENGINE_H
