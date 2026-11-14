#include "backupengine.h"
#include "core/exifscanner.h"
#include "core/flowpath.h"
#include "core/settingsmanager.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

static void backupLog(const QString &msg)
{
    QFile f(QDir::tempPath() + QStringLiteral("/kopal_backup.log"));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << "  " << msg << "\n";
    }
}
#include <QStorageInfo>
#include <QLocale>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QMutexLocker>
#include <QThread>

#include <algorithm>

namespace {

constexpr int kEmitIntervalMs = 80;

int resolveThreadCount()
{
    SettingsManager *sm = SettingsManager::instance();
    if (!sm->multithread())
        return 1;

    const int requested = sm->threadCount();
    if (requested > 0)
        return qBound(1, requested, 8);

    const int ideal = QThread::idealThreadCount();
    return qBound(2, qMax(2, ideal / 2), 4);
}

} // namespace

BackupEngine::BackupEngine(QObject *parent)
    : QObject(parent)
{
}

BackupEngine::~BackupEngine()
{
    stop();
    const QList<QPointer<BackupWorker>> workers = workersSnapshot();
    for (const QPointer<BackupWorker> &worker : workers) {
        if (worker && worker->isRunning())
            worker->wait(8000);
    }
}

QList<QPointer<BackupWorker>> BackupEngine::workersSnapshot()
{
    QMutexLocker lock(&m_mutex);
    return m_workers;
}

BackupEngine::State BackupEngine::state() const
{
    QMutexLocker lock(&m_mutex);
    return m_state;
}

BackupStats BackupEngine::stats() const
{
    QMutexLocker lock(&m_mutex);
    return m_stats;
}

QStringList BackupEngine::failedFiles() const
{
    QMutexLocker lock(&m_mutex);
    return m_failedFiles;
}

VerifyResult BackupEngine::verifyResult() const
{
    QMutexLocker lock(&m_mutex);
    return m_verify;
}

void BackupEngine::setState(State state)
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == state)
            return;
        m_state = state;
    }
    emit stateChanged(state);
}

void BackupEngine::start(const QString &source, const QString &target)
{
    if (state() == Running || state() == Paused)
        return;

    // 连点两次「开始传输」会同时进这里：collectFiles() 很慢，
    // 第二个调用会在第一个还没把状态切到 Running 时也进来，导致计划被清两遍
    {
        bool expected = false;
        if (!m_starting.compare_exchange_strong(expected, true))
            return;
    }
    struct StartGuard {
        std::atomic<bool> &flag;
        ~StartGuard() { flag = false; }
    } startGuard{m_starting};

    // 每次开跑都清零统计，避免「补拷」二次启动后出现 61/60 这种越界计数
    {
        QMutexLocker lock(&m_mutex);
        m_stats = BackupStats();
        m_verify = VerifyResult();
        m_failedFiles.clear();
    }

    // 主线程快照设置：后台线程不再碰 QSettings
    SettingsManager *sm = SettingsManager::instance();
    m_source = source;
    m_target = target;
    m_flowConfig = sm->flowConfig();
    m_autoRename = sm->autoRename();
    m_naming.prefix = sm->renamePrefix();
    m_naming.digits = sm->renameDigits();
    m_naming.custom = sm->renameCustom();
    m_legacySubfolderByExt = sm->subfolderByExt();
    m_legacyCategoryMode = sm->categoryMode();
    m_verifyEnabled = sm->verifyAfterBackup();
    m_threads = resolveThreadCount();

    {
        QMutexLocker lock(&m_mutex);
        m_paused = false;
        m_stopped = false;
        m_failedFiles.clear();
        m_verify = VerifyResult();
        m_workers.clear();
    }
    m_nextIndex.store(0);
    m_copyStarted = false;
    m_emitTimer = QElapsedTimer();

    // 防呆：目标目录必须能创建，不能等于源目录、也不能在源目录里面
    const QString targetClean = QDir::cleanPath(QDir(target).absolutePath());
    const QString sourceClean = QDir::cleanPath(QDir(source).absolutePath());
    if (targetClean.compare(sourceClean, Qt::CaseInsensitive) == 0) {
        setState(Idle);
        emit errorOccurred(QString("目标文件夹和源文件夹是同一个，换一个目标再试"));
        return;
    }
    if (targetClean.startsWith(sourceClean + QLatin1Char('/'), Qt::CaseInsensitive)) {
        setState(Idle);
        emit errorOccurred(QString("目标文件夹不能放在源文件夹里面"));
        return;
    }
    if (!QDir().mkpath(targetClean)) {
        setState(Idle);
        emit errorOccurred(QString("无法创建目标文件夹：%1").arg(targetClean));
        return;
    }

    collectFiles();

    if (m_stats.totalFiles == 0) {
        setState(Idle);
        emit errorOccurred(QString("源文件夹里没有找到可备份的文件：%1").arg(source));
        return;
    }

    // 空间不够就别开始，免得拷到一半全是失败
    {
        QStorageInfo storage(targetClean);
        if (storage.isValid()) {
            const qint64 available = storage.bytesAvailable();
            if (available > 0 && m_stats.totalBytes + (256LL << 20) > available) {
                setState(Idle);
                emit errorOccurred(QString("目标盘空间不够：需要 %1，可用 %2")
                                       .arg(QLocale().formattedDataSize(m_stats.totalBytes),
                                            QLocale().formattedDataSize(available)));
                return;
            }
        }
    }

    backupLog(QString("start: src=%1 dst=%2 files=%3 bytes=%4 threads=%5 verify=%6")
                  .arg(source, target).arg(m_stats.totalFiles).arg(m_stats.totalBytes)
                  .arg(m_threads).arg(m_verifyEnabled));

    setState(Running);

    m_emitTimer.start();

    auto *coordinator = new BackupWorker(this, true);
    connect(coordinator, &QThread::finished, coordinator, &QObject::deleteLater);
    {
        QMutexLocker lock(&m_mutex);
        m_workers.append(coordinator);
    }
    coordinator->start();
}

void BackupEngine::pause()
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == Running) {
            m_paused = true;
            m_state = Paused;
            changed = true;
        }
    }
    if (changed)
        emit stateChanged(Paused);
}

void BackupEngine::resume()
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_state == Paused) {
            m_paused = false;
            m_pauseCond.wakeAll();
            m_state = Running;
            changed = true;
        }
    }
    if (changed)
        emit stateChanged(Running);
}

void BackupEngine::stop()
{
    bool changed = false;
    {
        QMutexLocker lock(&m_mutex);
        m_stopped = true;
        m_paused = false;
        m_pauseCond.wakeAll();
        if (m_state == Running || m_state == Paused) {
            m_state = Idle;
            changed = true;
        }
    }
    if (changed)
        emit stateChanged(Idle);
}

void BackupEngine::collectFiles()
{
    m_plan.clear();
    m_stats = BackupStats();
    m_modelCache.clear();
    m_cameraFolders.reset();
    m_createdDirs.clear();

    QStringList paths;
    QDirIterator it(m_source, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString lower = info.fileName().toLower();
        if (lower == "thumbs.db" || lower == ".ds_store" || lower.startsWith("._"))
            continue;
        paths << info.absoluteFilePath();
    }

    paths.sort(Qt::CaseInsensitive);

    QVector<QFileInfo> infos;
    infos.reserve(paths.size());
    for (const QString &path : paths)
        infos.append(QFileInfo(path));

    m_totalFiles = infos.size();

    const bool wantsCamera = FlowPath::hasKey(m_flowConfig, FlowPath::Key::Camera);
    const bool wantsSeqKey = FlowPath::hasKey(m_flowConfig, FlowPath::Key::Seq);
    const int flowDigits = qMax(3, QString::number(qMax(1, m_totalFiles)).length());
    SequenceAllocator sequences;   // 方案 C：按目标文件夹分别从 1 开始

    m_plan.reserve(infos.size());
    for (int i = 0; i < infos.size(); ++i) {
        const QFileInfo &info = infos.at(i);

        PlannedFile planned;
        planned.info = info;
        // 只有流程里真的有「相机型号」节点时才去读 EXIF，否则不白读磁盘
        if (wantsCamera)
            planned.cameraModel = cameraModelFor(info);

        // 相机型号节点：已知型号直接用型号名；视频/截图读不到型号 -> 未知相机N
        QString cameraFolder;
        if (wantsCamera) {
            if (!planned.cameraModel.isEmpty())
                cameraFolder = m_cameraFolders.folderFor(planned.cameraModel);
            else
                cameraFolder = m_cameraFolders.unknownFolder(
                    info.absolutePath().toLower());
        }

        planned.relativeDir = relativeDirFor(info, cameraFolder);

        const QString suffix = info.suffix();
        if (m_autoRename) {
            // 序号按「相机型号 + 日期目录」分桶，换文件夹从 1 重新开始
            const QString folderKey = (cameraFolder.isEmpty() ? QStringLiteral("-") : cameraFolder)
                                      + QLatin1Char('|') + planned.relativeDir;
            QString base = m_naming.build(sequences.next(folderKey));
            planned.fileName = suffix.isEmpty() ? base : base + QLatin1Char('.') + suffix;
        } else if (wantsSeqKey) {
            planned.fileName = FlowPath::buildFileName(m_flowConfig, info.fileName(), QString(),
                                                       false, dateOf(info), i + 1, flowDigits);
        } else {
            planned.fileName = info.fileName();
        }

        const QString targetDir = planned.relativeDir.isEmpty()
                                      ? m_target
                                      : QDir(m_target).filePath(planned.relativeDir);
        planned.dstPath = QDir(targetDir).absoluteFilePath(planned.fileName);

        m_plan.append(planned);
        m_stats.totalFiles++;
        m_stats.totalBytes += info.size();
    }
}

// 读相机型号：按「文件所在目录 + 文件大小」缓存，避免同一张卡重复读
QString BackupEngine::cameraModelFor(const QFileInfo &info)
{
    const QString key = info.absolutePath().toLower() + QLatin1Char('|')
                        + QString::number(info.size());

    const auto cached = m_modelCache.constFind(key);
    if (cached != m_modelCache.constEnd())
        return cached.value();

    const QString model = ExifScanner::readCameraModel(info.absoluteFilePath());
    m_modelCache.insert(key, model);
    return model;
}

QDateTime BackupEngine::dateOf(const QFileInfo &info) const
{
    QDateTime dt = info.birthTime();
    if (!dt.isValid())
        dt = info.lastModified();
    if (!dt.isValid())
        dt = QDateTime::currentDateTime();
    return dt;
}

QString BackupEngine::relativeDirFor(const QFileInfo &info, const QString &cameraFolder) const
{
    if (!m_flowConfig.isEmpty())
        return FlowPath::relativeDir(m_flowConfig, info.fileName(), dateOf(info), cameraFolder);

    // 兼容旧版"分类方式 + 按后缀分子文件夹"
    const QDateTime dt = dateOf(info);
    QStringList parts;
    if (m_legacyCategoryMode == 0)
        parts << dt.toString("yyyy-MM-dd");
    else if (m_legacyCategoryMode == 1)
        parts << dt.toString("yyyy-MM");
    else
        parts << dt.toString("yyyy");

    if (m_legacySubfolderByExt) {
        const QString ext = info.suffix().toUpper();
        if (!ext.isEmpty())
            parts << ext;
    }
    return parts.join('/');
}

bool BackupEngine::copyFile(const QString &src, const QString &dst)
{
    // Windows 上刚生成的文件偶尔会被杀毒软件 / 索引服务短暂占用，
    // QFile::copy 会一次性失败。重试几次，别把偶发占用记成备份失败。
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (QFile::exists(dst) && !QFile::remove(dst)) {
            QThread::msleep(60);
            continue;
        }
        if (QFile::copy(src, dst))
            return true;
        QThread::msleep(60);
    }
    return false;
}

// 多线程下多个线程可能同时给同一个目录 mkpath，
// QDir::mkpath 在别人刚建好这一步时会返回 false（看起来像"创建文件夹失败"）。
// 这里串行化并记住已经建好的目录。
bool BackupEngine::ensureDir(const QString &dir)
{
    QMutexLocker lock(&m_dirMutex);
    if (m_createdDirs.contains(dir))
        return true;
    if (!QDir().mkpath(dir))
        return false;
    m_createdDirs.insert(dir);
    return true;
}

bool BackupEngine::copyOne(const PlannedFile &file)
{
    const QFileInfo &info = file.info;
    const QString srcPath = info.absoluteFilePath();
    const QString targetDir = file.relativeDir.isEmpty()
                                  ? m_target
                                  : QDir(m_target).filePath(file.relativeDir);

    if (!ensureDir(targetDir)) {
        {
            QMutexLocker lock(&m_mutex);
            m_failedFiles << srcPath;
            m_stats.failedCount++;
        }
        emit errorOccurred(QString("无法创建文件夹：%1").arg(targetDir));
        return false;
    }

    const QString dstPath = file.dstPath;

    // 已存在且大小相同的文件跳过
    if (QFileInfo::exists(dstPath)) {
        const QFileInfo dstInfo(dstPath);
        if (dstInfo.size() == info.size()) {
            {
                QMutexLocker lock(&m_mutex);
                m_stats.copiedFiles++;
                m_stats.copiedBytes += info.size();
            }
            return true;
        }
    }

    {
        QMutexLocker lock(&m_mutex);
        m_stats.currentFile = info.fileName();
    }

    if (copyFile(srcPath, dstPath)) {
        {
            QMutexLocker lock(&m_mutex);
            m_stats.copiedFiles++;
            m_stats.copiedBytes += info.size();
        }
        emit fileCopied(srcPath, dstPath);
        return true;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_failedFiles << srcPath;
        m_stats.failedCount++;
    }
    emit errorOccurred(QString("复制失败：%1").arg(info.fileName()));
    return false;
}

void BackupEngine::emitProgress(bool force)
{
    if (!force && m_emitTimer.isValid() && m_emitTimer.elapsed() < kEmitIntervalMs)
        return;

    if (m_emitTimer.isValid())
        m_emitTimer.restart();

    BackupStats snapshot;
    {
        QMutexLocker lock(&m_mutex);
        snapshot = m_stats;
    }

    const qint64 elapsed = m_runTimer.elapsed();
    if (elapsed > 0 && snapshot.copiedBytes > 0) {
        snapshot.speedMBps = double(snapshot.copiedBytes) / (1024.0 * 1024.0)
                             / (double(elapsed) / 1000.0);
        QMutexLocker lock(&m_mutex);
        m_stats.speedMBps = snapshot.speedMBps;
    }

    emit progressUpdated(snapshot);
}

void BackupEngine::copyLoop()
{
    for (;;) {
        {
            QMutexLocker lock(&m_mutex);
            if (m_stopped)
                return;
            while (m_paused)
                m_pauseCond.wait(&m_mutex);
            if (m_stopped)
                return;
        }

        const int index = m_nextIndex.fetch_add(1);
        if (index >= m_plan.size())
            return;
        m_copyStarted = true;

        copyOne(m_plan.at(index));
        emitProgress(false);
    }
}

void BackupEngine::verifyAll()
{
    VerifyResult result;
    result.ran = true;

    for (int i = 0; i < m_plan.size(); ++i) {
        {
            QMutexLocker lock(&m_mutex);
            if (m_stopped)
                return;
        }

        const PlannedFile &file = m_plan.at(i);
        const QString dstPath = file.dstPath;
        ++result.checked;

        emit verifyProgress(result.checked, m_plan.size());

        const QFileInfo dstInfo(dstPath);
        if (!dstInfo.exists() || dstInfo.size() != file.info.size()) {
            ++result.missing;
            if (result.missingFiles.size() < 200)
                result.missingFiles << dstPath;
            continue;
        }

        // 大小一致还不够：逐文件比对 SHA-256，确认内容一字不差
        const QString srcHash = sha256Of(file.info.absoluteFilePath());
        const QString dstHash = sha256Of(dstPath);
        if (srcHash.isEmpty() || dstHash.isEmpty() || srcHash != dstHash) {
            ++result.corrupt;
            if (result.corruptFiles.size() < 200)
                result.corruptFiles << dstPath;
        }
    }

    {
        QMutexLocker lock(&m_mutex);
        m_verify = result;
    }
    emit verified(result);
}

// 流式读文件算 SHA-256（1MB 一块，不占内存）
QString BackupEngine::sha256Of(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer;
    buffer.resize(1024 * 1024);
    while (!file.atEnd()) {
        const qint64 n = file.read(buffer.data(), buffer.size());
        if (n <= 0)
            break;
        hash.addData(QByteArrayView(buffer.constData(), int(n)));
    }
    return QString::fromLatin1(hash.result().toHex());
}

void BackupEngine::runBackup()
{
    m_runTimer.start();

    QList<BackupWorker *> extra;
    for (int i = 1; i < m_threads; ++i) {
        auto *worker = new BackupWorker(this, false);
        extra.append(worker);
    }

    for (BackupWorker *worker : extra) {
        {
            QMutexLocker lock(&m_mutex);
            m_workers.append(worker);
        }
        worker->start();
    }

    copyLoop();

    for (BackupWorker *worker : extra) {
        worker->wait();
        delete worker;
    }

    {
        QMutexLocker lock(&m_mutex);
        m_workers.erase(std::remove_if(m_workers.begin(), m_workers.end(),
                                       [](const QPointer<BackupWorker> &w) { return w.isNull(); }),
                        m_workers.end());
        if (m_stopped)
            return;
    }

    emitProgress(true);

    backupLog(QString("copy done: plan=%1 copied=%2 failed=%3 started=%4")
                  .arg(m_plan.size()).arg(m_stats.copiedFiles)
                  .arg(m_stats.failedCount).arg(m_copyStarted));

    // 一个都没拷、也没有失败记录：说明复制阶段压根没跑起来（内部状态异常）
    if (!m_copyStarted && !m_plan.isEmpty()) {
        backupLog("复制阶段没有执行：nextIndex 没有取到任务");
        {
            QMutexLocker lock(&m_mutex);
            m_state = Idle;
        }
        emit stateChanged(Idle);
        emit errorOccurred(QString("复制阶段没有执行（内部状态异常）。\n请把 %1 发给作者")
                               .arg(QDir::tempPath() + QStringLiteral("/kopal_backup.log")));
        return;
    }

    if (m_verifyEnabled)
        verifyAll();

    {
        QMutexLocker lock(&m_mutex);
        if (m_stopped)
            return;
        m_state = Completed;
    }
    emit stateChanged(Completed);
    emit completed();
}

// BackupWorker
BackupWorker::BackupWorker(BackupEngine *engine, bool coordinator, QObject *parent)
    : QThread(parent)
    , m_engine(engine)
    , m_coordinator(coordinator)
{
}

void BackupWorker::run()
{
    if (!m_engine)
        return;

    if (m_coordinator)
        m_engine->runBackup();
    else
        m_engine->copyLoop();
}
