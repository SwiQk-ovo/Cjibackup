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

static void checkKind(const QString &label, UpdateNoteKind actual, UpdateNoteKind expected)
{
    if (actual == expected) {
        g_out << "  OK   " << label << "\n";
    } else {
        ++g_failures;
        g_out << "  FAIL " << label << "（分类不对）\n";
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

    g_out << "[更新说明分类]\n";
    {
        const QVector<UpdateNote> notes = UpdateManager::parseReleaseNotes(QStringLiteral(
            "## 新增\n"
            "- 拖节点到节点上合并\n"
            "- 合并时选分隔符\n"
            "## 调整\n"
            "- 设置页重新归类\n"
            "## 修复\n"
            "- 切换分隔符后预览没变\n"
            "- 新增：这一条前缀应该覆盖所在小节\n"
            "\n"
            "**Full Changelog**: https://github.com/SwiQk-ovo/Cjibackup/compare/v2.0.2...v2.0.3\n"));

        checkInt("更新说明条数", notes.size(), 5);
        if (notes.size() == 5) {
            checkStr("新增小节第一条", notes.at(0).text, "拖节点到节点上合并");
            checkKind("新增小节第一条分类", notes.at(0).kind, UpdateNoteKind::Added);
            checkKind("新增小节第二条分类", notes.at(1).kind, UpdateNoteKind::Added);
            checkKind("调整小节分类", notes.at(2).kind, UpdateNoteKind::Changed);
            checkKind("修复小节分类", notes.at(3).kind, UpdateNoteKind::Fixed);
            checkStr("行内前缀去掉", notes.at(4).text, "这一条前缀应该覆盖所在小节");
            checkKind("行内前缀覆盖小节", notes.at(4).kind, UpdateNoteKind::Added);
        }
    }

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
