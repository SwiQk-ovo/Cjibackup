#include "historymanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QDebug>

HistoryManager* HistoryManager::instance()
{
    static HistoryManager inst;
    return &inst;
}

HistoryManager::HistoryManager(QObject *parent)
    : QObject(parent)
{
    QString dataDir = QDir::homePath() + "/.cjibackup";
    QDir().mkpath(dataDir);
    m_filePath = dataDir + "/history.json";
    load();
}

QVector<HistoryRecord> HistoryManager::records() const
{
    return m_records;
}

void HistoryManager::addRecord(const HistoryRecord &record)
{
    m_records.prepend(record);
    if (m_records.size() > 100) m_records.remove(100, m_records.size() - 100);
    save();
    emit recordsChanged();
}

void HistoryManager::clearAll()
{
    m_records.clear();
    save();
    emit recordsChanged();
}

void HistoryManager::load()
{
    QFile f(m_filePath);
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        QJsonObject obj = v.toObject();
        HistoryRecord rec;
        rec.startTime = QDateTime::fromString(obj["start"].toString(), Qt::ISODate);
        rec.endTime = QDateTime::fromString(obj["end"].toString(), Qt::ISODate);
        rec.fileCount = obj["count"].toInt();
        rec.totalBytes = (qint64)obj["bytes"].toDouble();
        rec.sourcePath = obj["source"].toString();
        rec.targetPath = obj["target"].toString();
        rec.cameraModel = obj["camera"].toString();
        rec.formatted = obj["formatted"].toBool();
        rec.avgSpeed = obj["speed"].toDouble();
        m_records.append(rec);
    }
}

void HistoryManager::save()
{
    QJsonArray arr;
    for (const HistoryRecord &rec : m_records) {
        QJsonObject obj;
        obj["start"] = rec.startTime.toString(Qt::ISODate);
        obj["end"] = rec.endTime.toString(Qt::ISODate);
        obj["count"] = rec.fileCount;
        obj["bytes"] = (double)rec.totalBytes;
        obj["source"] = rec.sourcePath;
        obj["target"] = rec.targetPath;
        obj["camera"] = rec.cameraModel;
        obj["formatted"] = rec.formatted;
        obj["speed"] = rec.avgSpeed;
        arr.append(obj);
    }

    QFile f(m_filePath);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson());
        f.close();
    }
}
