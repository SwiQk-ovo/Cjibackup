#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QObject>
#include <QVector>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

struct HistoryRecord {
    QDateTime startTime;
    QDateTime endTime;
    int fileCount = 0;
    qint64 totalBytes = 0;
    QString sourcePath;
    QString targetPath;
    QString cameraModel;
    bool formatted = false;
    double avgSpeed = 0;
};

class HistoryManager : public QObject
{
    Q_OBJECT

public:
    static HistoryManager* instance();

    QVector<HistoryRecord> records() const;
    void addRecord(const HistoryRecord &record);
    void clearAll();
    // 撤销「清空记录」用：整份写回
    void setRecords(const QVector<HistoryRecord> &records);

signals:
    void recordsChanged();

private:
    explicit HistoryManager(QObject *parent = nullptr);
    void load();
    void save();

    QVector<HistoryRecord> m_records;
    QString m_filePath;
};

#endif // HISTORYMANAGER_H
