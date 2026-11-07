#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QVector>

#include <functional>

/**
 * 安装包载荷读取：
 * 安装器 exe 末尾依次是 [原 exe][payload][footer]
 * footer = quint64 payloadSize + "CJISFX01"
 * payload = "CJIPK002" + quint32 count + quint64 totalRaw + 条目...
 * 条目 = quint16 pathLen + path(utf8) + quint64 rawSize + quint32 compSize + qCompress 数据
 */
class PayloadReader
{
public:
    struct Entry
    {
        QString path;
        quint64 rawSize = 0;
        quint32 compSize = 0;
    };

    bool open(const QString &exePath, QString *error);
    bool isOpen() const { return m_open; }
    int count() const { return m_entries.size(); }
    quint64 totalRawBytes() const { return m_totalRaw; }
    const QVector<Entry> &entries() const { return m_entries; }

    using ProgressFn = std::function<void(int index, const QString &path)>;
    bool extractAll(const QString &targetDir, const ProgressFn &progress, QString *error);

private:
    QFile m_file;
    bool m_open = false;
    quint64 m_payloadOffset = 0;
    QVector<Entry> m_entries;
    quint64 m_totalRaw = 0;
};

#endif // PAYLOAD_H
