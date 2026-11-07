#include "appdirs.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace Cji {

QString dataDir()
{
    return QDir::homePath() + QStringLiteral("/.kopal");
}

void migrateLegacyDataDir()
{
    const QString legacy = QDir::homePath() + QStringLiteral("/.cjibackup");
    const QString target = dataDir();


    // 注意：这里用 QDir/QFile 判断，避免 QFileInfo::exists 在部分环境下误报
    if (!QDir(legacy).exists())
        return;

    // 新目录已经有数据就不动；只有在「新目录不存在或空」时才搬
    if (QDir(target).exists()) {
        const QFileInfoList existing = QDir(target).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        if (!existing.isEmpty())
            return;
    }

    // 优先整体改名（同一个分区，瞬时完成）；失败再退回逐文件复制
    if (QDir().rename(legacy, target))
        return;

    if (!QDir().mkpath(target))
        return;

    QDir source(legacy);
    const QFileInfoList files = source.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    bool allCopied = true;
    for (const QFileInfo &info : files) {
        const QString dst = target + QLatin1Char('/') + info.fileName();
        if (!QFile::copy(info.absoluteFilePath(), dst))
            allCopied = false;
    }

    if (allCopied)
        QDir(legacy).removeRecursively();   // 全部搬完才删老目录
}

} // namespace Cji
