#include "payload.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>

#include <cstring>

namespace {
constexpr char kFooterMagic[8] = {'C', 'J', 'I', 'S', 'F', 'X', '0', '1'};
constexpr char kPayloadMagic[8] = {'C', 'J', 'I', 'P', 'K', '0', '0', '2'};
constexpr int kFooterSize = 16;
} // namespace

bool PayloadReader::open(const QString &exePath, QString *error)
{
    m_open = false;
    m_entries.clear();
    m_totalRaw = 0;
    m_payloadOffset = 0;

    // 优先读同目录下的 payload.pack（引导程序解出来的情况）
    const QString sidecar = QFileInfo(exePath).absolutePath() + QStringLiteral("/payload.pack");
    const bool useSidecar = QFileInfo::exists(sidecar);

    m_file.setFileName(useSidecar ? sidecar : exePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法读取安装数据");
        return false;
    }

    if (!useSidecar) {
        const qint64 fileSize = m_file.size();
        if (fileSize < kFooterSize + 32) {
            if (error)
                *error = QStringLiteral("安装包不完整（缺少安装数据）");
            return false;
        }

        if (!m_file.seek(fileSize - kFooterSize)) {
            if (error)
                *error = QStringLiteral("安装包读取失败");
            return false;
        }

        QDataStream footer(&m_file);
        footer.setByteOrder(QDataStream::LittleEndian);
        quint64 payloadSize = 0;
        footer >> payloadSize;
        char magic[8] = {0};
        footer.readRawData(magic, 8);

        if (std::memcmp(magic, kFooterMagic, 8) != 0 || payloadSize == 0) {
            if (error)
                *error = QStringLiteral("这个文件不是完整的安装包，请重新下载");
            return false;
        }
        m_payloadOffset = quint64(fileSize - kFooterSize) - payloadSize;
    }

    m_file.seek(qint64(m_payloadOffset));
    QDataStream in(&m_file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_0);

    char pmagic[8] = {0};
    in.readRawData(pmagic, 8);
    if (std::memcmp(pmagic, kPayloadMagic, 8) != 0) {
        if (error)
            *error = QStringLiteral("安装数据校验失败，请重新下载");
        return false;
    }

    quint32 count = 0;
    quint64 totalRaw = 0;
    in >> count >> totalRaw;
    if (count == 0 || count > 100000) {
        if (error)
            *error = QStringLiteral("安装数据条目异常");
        return false;
    }

    for (quint32 i = 0; i < count; ++i) {
        quint16 pathLen = 0;
        in >> pathLen;
        if (pathLen == 0 || pathLen > 4096) {
            if (error)
                *error = QStringLiteral("安装数据条目异常");
            return false;
        }

        QByteArray pathBytes(int(pathLen), Qt::Uninitialized);
        in.readRawData(pathBytes.data(), pathLen);

        quint64 rawSize = 0;
        quint32 compSize = 0;
        in >> rawSize >> compSize;
        if (in.status() != QDataStream::Ok) {
            if (error)
                *error = QStringLiteral("安装数据读取中断");
            return false;
        }

        Entry entry;
        entry.path = QString::fromUtf8(pathBytes);
        entry.rawSize = rawSize;
        entry.compSize = compSize;
        m_entries.append(entry);

        if (!m_file.seek(m_file.pos() + compSize)) {
            if (error)
                *error = QStringLiteral("安装数据读取中断");
            return false;
        }
    }

    m_totalRaw = totalRaw;
    m_open = true;
    return true;
}

bool PayloadReader::extractAll(const QString &targetDir, const ProgressFn &progress, QString *error)
{
    if (!m_open)
        return false;

    if (!m_file.seek(qint64(m_payloadOffset))) {
        if (error)
            *error = QStringLiteral("安装数据定位失败");
        return false;
    }

    QDataStream in(&m_file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_0);

    char magic[8] = {0};
    in.readRawData(magic, 8);
    quint32 count = 0;
    quint64 totalRaw = 0;
    in >> count >> totalRaw;

    QDir().mkpath(targetDir);

    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &entry = m_entries.at(i);

        quint16 pathLen = 0;
        in >> pathLen;
        QByteArray pathBytes(int(pathLen), Qt::Uninitialized);
        in.readRawData(pathBytes.data(), pathLen);

        quint64 rawSize = 0;
        quint32 compSize = 0;
        in >> rawSize >> compSize;

        QByteArray compressed(int(compSize), Qt::Uninitialized);
        if (in.readRawData(compressed.data(), int(compSize)) != qint64(compSize)) {
            if (error)
                *error = QStringLiteral("安装数据读取中断：%1").arg(entry.path);
            return false;
        }

        const QByteArray data = qUncompress(compressed);
        if (quint64(data.size()) != entry.rawSize) {
            if (error)
                *error = QStringLiteral("文件解压失败：%1").arg(entry.path);
            return false;
        }

        const QString targetPath = QDir(targetDir).filePath(entry.path);
        QDir().mkpath(QFileInfo(targetPath).absolutePath());

        QFile out(targetPath);
        if (!out.open(QIODevice::WriteOnly)) {
            if (error)
                *error = QStringLiteral("无法写入：%1").arg(targetPath);
            return false;
        }
        out.write(data);
        out.close();

        if (progress)
            progress(i, entry.path);
    }

    return true;
}
