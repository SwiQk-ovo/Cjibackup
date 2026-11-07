// 版本比较 + 定时提醒调度的单元测试
#include "core/remindermanager.h"
#include "core/settingsmanager.h"
#include "core/updatemanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QTextStream>

static int g_failures = 0;
static QTextStream g_out(stdout);

static void checkInt(const QString &label, int actual, int expected)
{
    if (actual == expected) {
        g_out << "  OK   " << label << " = " << actual << "\n";
    } else {
        ++g_failures;
        g_out << "  FAIL " << label << " = " << actual << "，期望 " << expected << "\n";
    }
}

static void checkStr(const QString &label, const QString &actual, const QString &expected)
{
    if (actual == expected) {
        g_out << "  OK   " << label << " = " << actual << "\n";
    } else {
        ++g_failures;
        g_out << "  FAIL " << label << " = [" << actual << "]，期望 [" << expected << "]\n";
    }
}

static void checkTrue(const QString &label, bool value)
{
    if (value) {
        g_out << "  OK   " << label << "\n";
    } else {
        ++g_failures;
        g_out << "  FAIL " << label << "\n";
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Cjibackup");
    app.setApplicationName("CjiCoreTest");

    g_out << "[版本号]\n";
    checkStr("normalize 测试前缀", UpdateManager::normalizeVersion("测试 v0.3.0"), "0.3.0");
    checkStr("normalize 纯数字", UpdateManager::normalizeVersion("1.2"), "1.2.0");
    checkStr("normalize v 前缀", UpdateManager::normalizeVersion("v2.10.3"), "2.10.3");

    checkInt("0.3.0 vs 0.3.0", UpdateManager::compareVersions("0.3.0", "0.3.0"), 0);
    checkInt("0.3.0 vs 0.4.0", UpdateManager::compareVersions("0.3.0", "0.4.0"), -1);
    checkInt("0.4.0 vs 0.3.0", UpdateManager::compareVersions("0.4.0", "0.3.0"), 1);
    checkInt("0.10.0 vs 0.9.9", UpdateManager::compareVersions("0.10.0", "0.9.9"), 1);
    checkInt("1.0 vs 1.0.0", UpdateManager::compareVersions("1.0", "1.0.0"), 0);
    checkInt("测试 v1.2.3 vs 正式 v1.2.2",
             UpdateManager::compareVersions("测试 v1.2.3", "正式 v1.2.2"), 1);

    g_out << "[定时提醒]\n";
    SettingsManager *sm = SettingsManager::instance();
    sm->setReminderEnabled(false);
    ReminderManager manager;
    checkTrue("关闭时没有下次提醒", !manager.nextDue().isValid());

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime later = now.addSecs(90);
    sm->setReminderEnabled(true);
    sm->setReminderMode("daily");
    sm->setReminderTime(later.toString("HH:mm"));

    const QDateTime daily = manager.nextDue();
    checkTrue("每日提醒有下次时间", daily.isValid());
    checkTrue("下次时间不早于现在", daily >= later.addSecs(-60));
    checkTrue("下次时间在 25 小时内", now.secsTo(daily) <= 25 * 3600);

    sm->setReminderMode("weekly");
    sm->setReminderWeekday(now.date().dayOfWeek());
    const QDateTime weekly = manager.nextDue();
    checkTrue("每周提醒有下次时间", weekly.isValid());
    checkTrue("每周提醒在 8 天内", now.secsTo(weekly) <= 8 * 24 * 3600);

    sm->setReminderMode("once");
    sm->setReminderDate(now.date().addDays(-1).toString("yyyy-MM-dd"));
    sm->setReminderTime("09:00");
    checkTrue("只提醒一次且日期已过则无下次", !manager.nextDue().isValid());

    sm->setReminderDate(now.date().addDays(1).toString("yyyy-MM-dd"));
    checkTrue("只提醒一次且在明天", manager.nextDue().isValid());

    sm->setReminderEnabled(false);
    QSettings clean(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    clean.clear();

    g_out << "\n";
    if (g_failures == 0) {
        g_out << "全部通过\n";
        return 0;
    }
    g_out << "失败 " << g_failures << " 项\n";
    return 1;
}
