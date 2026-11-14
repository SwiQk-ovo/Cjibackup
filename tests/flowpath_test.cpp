#include "core/flowpath.h"
#include "core/cameranaming.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QStringList>
#include <QTextStream>
#include <QTime>

static int g_failures = 0;

static void check(const QString &label, const QString &actual, const QString &expected)
{
    QTextStream out(stdout);
    if (actual == expected) {
        out << "  OK   " << label << " => " << actual << "\n";
    } else {
        ++g_failures;
        out << "  FAIL " << label << " => got [" << actual << "], want [" << expected << "]\n";
    }
    out.flush();
}

static void checkTrue(const QString &label, bool value)
{
    QTextStream out(stdout);
    if (value) {
        out << "  OK   " << label << "\n";
    } else {
        ++g_failures;
        out << "  FAIL " << label << "\n";
    }
    out.flush();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QDateTime when(QDate(2026, 10, 31), QTime(15, 4, 5));

    out << "[目录层级]\n";

    QStringList cfg{"year", "month", "event:校运会", "ext"};
    check("年/月/事件/后缀", FlowPath::relativeDir(cfg, "IMG_0001.JPG", when), "2026-10/校运会/JPG");

    cfg = {"year", "month", "day", "ext"};
    check("年/月/日/后缀", FlowPath::relativeDir(cfg, "a.CR3", when), "2026-10-31/CR3");

    cfg = {"month", "day"};
    check("只有月/日", FlowPath::relativeDir(cfg, "a.jpg", when), "10-31");

    cfg = {"year", "day"};
    check("年/日（中间没有月）", FlowPath::relativeDir(cfg, "a.jpg", when), "2026-31");

    cfg = {"month", "year", "day"};
    check("月/年/日（顺序不同）", FlowPath::relativeDir(cfg, "a.jpg", when), "10-2026-31");

    cfg = {"date", "ext"};
    check("旧版 date 键", FlowPath::relativeDir(cfg, "a.JPG", when), "2026-10-31/JPG");

    cfg = {"custom:旅行/2026", "ext"};
    check("自定义含非法字符", FlowPath::relativeDir(cfg, "a.jpg", when), "旅行_2026/JPG");

    cfg = {"event:"};
    check("空事件名", FlowPath::relativeDir(cfg, "a.jpg", when), "未命名事件");

    cfg = {"seq"};
    check("只有序号不建目录", FlowPath::relativeDir(cfg, "a.jpg", when), "");

    cfg = {"ext"};
    check("无后缀文件", FlowPath::relativeDir(cfg, "README", when), "OTHER");

    out << "[合并节点]\n";

    cfg = {FlowPath::encodeGroup(QStringLiteral("-"), {"year", "month"}), "ext"};
    check("年+月合并成一层", FlowPath::relativeDir(cfg, "a.jpg", when), "2026-10/JPG");

    cfg = {FlowPath::encodeGroup(QStringLiteral("."), {"year", "month", "day"})};
    check("年+月+日用 . 连接", FlowPath::relativeDir(cfg, "a.jpg", when), "2026.10.31");

    cfg = {FlowPath::encodeGroup(QStringLiteral(" "), {"year", "month"})};
    check("用空格连接", FlowPath::relativeDir(cfg, "a.jpg", when), "2026 10");

    cfg = {FlowPath::encodeGroup(QStringLiteral("_"), {"event:校运会", "ext"})};
    check("事件+后缀合并", FlowPath::relativeDir(cfg, "a.CR3", when), "校运会_CR3");

    cfg = {FlowPath::encodeGroup(QStringLiteral("-"), {"year", "month", "day", "camera", "ext"})};
    check("日期+相机+后缀合并成一层",
          FlowPath::relativeDir(cfg, "a.NEF", when, QStringLiteral("Nikon Z 6II")),
          "2026-10-31-Nikon Z 6II-NEF");

    cfg = {"year", "month", FlowPath::encodeGroup(QStringLiteral("."), {"day", "event:校运会"}),
           "ext"};
    check("合并节点前后的日期段分开",
          FlowPath::relativeDir(cfg, "a.jpg", when), "2026-10/31.校运会/JPG");

    checkTrue("merge: 开头的配置能被识别",
              FlowPath::isMergeEntry(FlowPath::encodeGroup(QStringLiteral("-"), {"year"})));
    checkTrue("普通配置不会被当成合并节点", !FlowPath::isMergeEntry("year"));

    {
        const QString entry = FlowPath::encodeGroup(QStringLiteral("."), {"year", "month", "day"});
        const QStringList roundTrip =
            FlowPath::serialize(FlowPath::parse({"year", entry, "event:婚礼"}));
        check("合并配置序列化往返", roundTrip.join("|"),
              QStringLiteral("year|%1|event:婚礼").arg(entry));
    }

    {
        FlowPath::setSeparator(QStringLiteral("."));
        check("全局日期分隔符切换", FlowPath::relativeDir({"year", "month", "day"}, "a.jpg", when),
              "2026.10.31");
        FlowPath::setSeparator(QStringLiteral("-"));
        check("全局日期分隔符恢复", FlowPath::relativeDir({"year", "month", "day"}, "a.jpg", when),
              "2026-10-31");
    }

    out << "[相机型号]\n";

    cfg = {"year", "month", "day", "camera", "ext"};
    check("年/月/日/相机/后缀",
          FlowPath::relativeDir(cfg, "DSC_0001.NEF", when, QStringLiteral("Nikon Z 6II")),
          "2026-10-31/Nikon Z 6II/NEF");

    check("相机型号缺失时回退成未知相机",
          FlowPath::relativeDir(cfg, "MOV_0001.MP4", when, QString()),
          "2026-10-31/未知相机/MP4");

    check("相机型号里的非法字符被替换",
          FlowPath::relativeDir(cfg, "a.JPG", when, QStringLiteral("Sony A|7")),
          "2026-10-31/Sony A_7/JPG");

    cfg = {"camera", "ext"};
    check("只有相机型号",
          FlowPath::relativeDir(cfg, "a.CR3", when, QStringLiteral("Canon EOS R6")),
          "Canon EOS R6/CR3");

    out << "[文件名]\n";

    check("序号写进文件名",
          FlowPath::buildFileName({"year", "seq"}, "IMG_0001.JPG", QString(), false, when, 7, 4),
          "0007_IMG_0001.JPG");

    check("自动重命名",
          FlowPath::buildFileName({"year"}, "IMG_0001.JPG", "{brand}-{date}-{seq}", true, when, 7, 3),
          "Camera-20261031-007.JPG");

    check("保留原文件名",
          FlowPath::buildFileName({"year"}, "IMG_0001.JPG", QString(), false, when, 7, 3),
          "IMG_0001.JPG");

    out << "[序列化]\n";

    check("序列化往返",
          FlowPath::serialize(FlowPath::parse({"year", "event:婚礼", "custom:2026-旅行"})).join("|"),
          "year|event:婚礼|custom:2026-旅行");

    check("相机节点序列化往返",
          FlowPath::serialize(FlowPath::parse({"camera", "year", "ext"})).join("|"),
          "camera|year|ext");

    out << "[文件重命名]\n";

    {
        NamingPlan plan;
        plan.prefix = QStringLiteral("IMG");
        plan.digits = 4;
        check("前缀 + 4 位序号", plan.build(1), "IMG0001");

        plan.prefix = QStringLiteral("照片");
        plan.digits = 5;
        plan.custom = QStringLiteral("婚礼");
        check("前缀 + 5 位序号 + 自定义内容", plan.build(23), "照片00023婚礼");

        plan.prefix = QString();
        plan.custom = QString();
        plan.digits = 3;
        check("前缀留空时也有序号", plan.build(7), "007");

        plan.prefix = QStringLiteral("A/B");
        plan.custom = QStringLiteral("C:D");
        plan.digits = 4;
        check("文件名里的非法字符被替换", plan.build(2), "A_B0002C_D");
    }

    out << "[序号 / 相机文件夹分配]\n";

    {
        // 方案 C：按目标文件夹分别计数
        SequenceAllocator sequences;
        check("同一文件夹内递增",
              QString("%1|%2|%3")
                  .arg(sequences.next("Nikon Z 6II|2026/10/31"))
                  .arg(sequences.next("Nikon Z 6II|2026/10/31"))
                  .arg(sequences.next("Sony A7M4|2026/10/31")),
              "1|2|1");
    }

    {
        CameraFolderAllocator folders;
        check("已知型号直接用型号名", folders.folderFor("Nikon Z 6II"), "Nikon Z 6II");
        check("同型号复用同一文件夹", folders.folderFor("Nikon Z 6II"), "Nikon Z 6II");
        check("未知来源从 1 递增", folders.unknownFolder("dcim/100canon"), "未知相机1");
        check("同一个未知来源复用编号", folders.unknownFolder("dcim/100canon"), "未知相机1");
        check("另一个未知来源递增", folders.unknownFolder("dcim/101canon"), "未知相机2");
    }

    out << "[预览]\n";

    check("预览示例", FlowPath::preview({"year", "month", "day", "ext"}),
          "2026-10-31/JPG/IMG_0001.JPG");

    check("带相机型号的预览",
          FlowPath::preview({"year", "month", "day", "camera", "ext"}, QStringLiteral("Nikon Z 6II")),
          "2026-10-31/Nikon Z 6II/JPG/IMG_0001.JPG");

    check("合并节点的预览",
          FlowPath::preview({FlowPath::encodeGroup(QStringLiteral("."), {"year", "month", "day"}),
                             "ext"}),
          "2026.10.31/JPG/IMG_0001.JPG");

    out << "\n";
    if (g_failures == 0) {
        out << "全部通过\n";
        return 0;
    }
    out << "失败 " << g_failures << " 项\n";
    return 1;
}
