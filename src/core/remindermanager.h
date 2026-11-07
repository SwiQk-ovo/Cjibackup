#ifndef REMINDERMANAGER_H
#define REMINDERMANAGER_H

#include <QDateTime>
#include <QObject>

class QTimer;

/**
 * 定时备份提醒：按设置算出最近一次应提醒时间，到点发信号。
 * 只负责提醒，不自动开始备份。
 */
class ReminderManager : public QObject
{
    Q_OBJECT

public:
    explicit ReminderManager(QObject *parent = nullptr);

    void start();
    void stop();

    // 下一次提醒时间（没有启用时返回无效 QDateTime）
    QDateTime nextDue() const;

signals:
    void reminderTriggered(const QDateTime &due);

private slots:
    void check();

private:
    QDateTime mostRecentDue(const QDateTime &now) const;
    QDateTime previousDue(const QDateTime &now) const;

    QTimer *m_timer = nullptr;
};

#endif // REMINDERMANAGER_H
