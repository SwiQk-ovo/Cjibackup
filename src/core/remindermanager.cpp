#include "remindermanager.h"

#include "settingsmanager.h"

#include <QTime>
#include <QTimer>

namespace {

constexpr int kCheckIntervalMs = 30 * 1000;
constexpr int kFreshWindowSeconds = 6 * 60 * 60;   // 超过 6 小时的过期提醒不再弹

} // namespace

ReminderManager::ReminderManager(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(kCheckIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &ReminderManager::check);
}

void ReminderManager::start()
{
    if (!m_timer->isActive())
        m_timer->start();
    check();
}

void ReminderManager::stop()
{
    m_timer->stop();
}

QDateTime ReminderManager::mostRecentDue(const QDateTime &now) const
{
    SettingsManager *sm = SettingsManager::instance();
    if (!sm->reminderEnabled())
        return QDateTime();

    const QString mode = sm->reminderMode();
    const QTime time = QTime::fromString(sm->reminderTime(), "HH:mm");
    if (!time.isValid())
        return QDateTime();

    const QDate today = now.date();

    if (mode == QStringLiteral("daily")) {
        QDateTime due(today, time);
        if (due > now)
            due = due.addDays(-1);
        return due;
    }

    if (mode == QStringLiteral("weekly")) {
        const int targetIso = qBound(1, sm->reminderWeekday(), 7);
        const int todayIso = today.dayOfWeek();
        int delta = todayIso - targetIso;
        QDateTime due(today.addDays(-delta), time);
        if (due > now)
            due = due.addDays(-7);
        return due;
    }

    if (mode == QStringLiteral("monthly")) {
        const QDate anchor = QDate::fromString(sm->reminderDate(), "yyyy-MM-dd");
        const int day = anchor.isValid() ? anchor.day() : today.day();
        QDate base(today.year(), today.month(), qMin(day, today.daysInMonth()));
        QDateTime due(base, time);
        if (due > now) {
            const QDate prevMonth = QDate(today.year(), today.month(), 1).addDays(-1);
            base = QDate(prevMonth.year(), prevMonth.month(), qMin(day, prevMonth.daysInMonth()));
            due = QDateTime(base, time);
        }
        return due;
    }

    if (mode == QStringLiteral("yearly")) {
        const QDate anchor = QDate::fromString(sm->reminderDate(), "yyyy-MM-dd");
        const int month = anchor.isValid() ? anchor.month() : today.month();
        const int day = anchor.isValid() ? anchor.day() : today.day();
        QDate base(today.year(), month, qMin(day, QDate(today.year(), month, 1).daysInMonth()));
        QDateTime due(base, time);
        if (due > now) {
            const int prevYear = today.year() - 1;
            base = QDate(prevYear, month, qMin(day, QDate(prevYear, month, 1).daysInMonth()));
            due = QDateTime(base, time);
        }
        return due;
    }

    // once
    const QDate anchor = QDate::fromString(sm->reminderDate(), "yyyy-MM-dd");
    if (!anchor.isValid())
        return QDateTime();
    const QDateTime due(anchor, time);
    return due <= now ? due : QDateTime();
}

QDateTime ReminderManager::previousDue(const QDateTime &now) const
{
    const QDateTime due = mostRecentDue(now);
    if (!due.isValid())
        return QDateTime();
    if (now.secsTo(due) < -kFreshWindowSeconds)
        return QDateTime();
    return due;
}

QDateTime ReminderManager::nextDue() const
{
    SettingsManager *sm = SettingsManager::instance();
    if (!sm->reminderEnabled())
        return QDateTime();

    const QDateTime now = QDateTime::currentDateTime();
    const QString mode = sm->reminderMode();
    const QTime time = QTime::fromString(sm->reminderTime(), "HH:mm");
    if (!time.isValid())
        return QDateTime();

    const QDate today = now.date();

    if (mode == QStringLiteral("daily")) {
        QDateTime due(today, time);
        if (due <= now)
            due = due.addDays(1);
        return due;
    }

    if (mode == QStringLiteral("weekly")) {
        const int targetIso = qBound(1, sm->reminderWeekday(), 7);
        int delta = targetIso - today.dayOfWeek();
        if (delta < 0)
            delta += 7;
        QDateTime due(today.addDays(delta), time);
        if (due <= now)
            due = due.addDays(7);
        return due;
    }

    if (mode == QStringLiteral("monthly")) {
        const QDate anchor = QDate::fromString(sm->reminderDate(), "yyyy-MM-dd");
        const int day = anchor.isValid() ? anchor.day() : today.day();
        QDate base(today.year(), today.month(), qMin(day, today.daysInMonth()));
        QDateTime due(base, time);
        if (due <= now) {
            const QDate nextMonth = QDate(today.year(), today.month(), 1).addMonths(1);
            base = QDate(nextMonth.year(), nextMonth.month(),
                         qMin(day, nextMonth.daysInMonth()));
            due = QDateTime(base, time);
        }
        return due;
    }

    if (mode == QStringLiteral("yearly")) {
        const QDate anchor = QDate::fromString(sm->reminderDate(), "yyyy-MM-dd");
        const int month = anchor.isValid() ? anchor.month() : today.month();
        const int day = anchor.isValid() ? anchor.day() : today.day();
        QDate base(today.year(), month, qMin(day, QDate(today.year(), month, 1).daysInMonth()));
        QDateTime due(base, time);
        if (due <= now) {
            base = QDate(today.year() + 1, month,
                         qMin(day, QDate(today.year() + 1, month, 1).daysInMonth()));
            due = QDateTime(base, time);
        }
        return due;
    }

    const QDate anchor = QDate::fromString(sm->reminderDate(), "yyyy-MM-dd");
    if (!anchor.isValid())
        return QDateTime();
    const QDateTime due(anchor, time);
    return due > now ? due : QDateTime();
}

void ReminderManager::check()
{
    SettingsManager *sm = SettingsManager::instance();
    if (!sm->reminderEnabled())
        return;

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime due = previousDue(now);
    if (!due.isValid())
        return;

    const QString lastFired = sm->reminderLastFired();
    const QDateTime fired = QDateTime::fromString(lastFired, Qt::ISODate);

    if (sm->reminderMode() == QStringLiteral("once")) {
        if (fired.isValid())
            return;   // 仅一次：已经提醒过就不再提醒
    } else if (fired.isValid() && fired >= due) {
        return;
    }

    sm->setReminderLastFired(now.toString(Qt::ISODate));
    emit reminderTriggered(due);
}
