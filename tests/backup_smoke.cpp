// 备份引擎端到端冒烟测试：验证流程图配置 -> 真实目录层级
#include "core/backupengine.h"
#include "core/exifscanner.h"
#include "core/settingsmanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>
#include <QThread>

static int g_failures = 0;
static QTextStream g_out(stdout);

static void makeFile(const QString &path, const QDateTime &when, const QByteArray &content = "abc")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        g_out << "  FAIL 无法写入 " << path << "\n";
        ++g_failures;
        return;
    }
    file.write(content);
    file.close();

    if (file.open(QIODevice::ReadWrite)) {
        file.setFileTime(when, QFileDevice::FileModificationTime);
        file.setFileTime(when, QFileDevice::FileBirthTime);
        file.close();
    }
}

static void expectFile(const QString &path)
{
    if (QFileInfo::exists(path)) {
        g_out << "  OK   " << QDir::toNativeSeparators(path) << "\n";
    } else {
        ++g_failures;
        g_out << "  FAIL 缺少 " << QDir::toNativeSeparators(path) << "\n";
    }
}

// 造一个最小的 TIFF 头，只带 Make / Model 两个 ASCII 标签，
// 让 ExifScanner 能像读 DNG / NEF 一样读出相机型号。
static QByteArray fakeTiffWithModel(const QByteArray &make, const QByteArray &model)
{
    const QByteArray makeData = make + '\0';
    const QByteArray modelData = model + '\0';
    const quint16 entryCount = 2;
    const quint32 ifdOffset = 8;
    const quint32 dataOffset = ifdOffset + 2 + quint32(entryCount) * 12 + 4;

    QByteArray out;
    auto putU16 = [&out](quint16 v) {
        out.append(char(v & 0xFF));
        out.append(char((v >> 8) & 0xFF));
    };
    auto putU32 = [&out](quint32 v) {
        out.append(char(v & 0xFF));
        out.append(char((v >> 8) & 0xFF));
        out.append(char((v >> 16) & 0xFF));
        out.append(char((v >> 24) & 0xFF));
    };

    out.append("II");            // little-endian
    putU16(42);                  // magic
    putU32(ifdOffset);           // 第一个 IFD 的偏移
    putU16(entryCount);

    putU16(0x010F);              // Make
    putU16(2);                   // ASCII
    putU32(quint32(makeData.size()));
    putU32(dataOffset);

    putU16(0x0110);              // Model
    putU16(2);
    putU32(quint32(modelData.size()));
    putU32(dataOffset + quint32(makeData.size()));

    putU32(0);                   // 没有下一个 IFD
    out.append(makeData);
    out.append(modelData);
    return out;
}

static bool runEngine(const QString &src, const QString &dst)
{
    BackupEngine engine;
    engine.start(src, dst);

    for (int i = 0; i < 2000; ++i) {
        const BackupEngine::State state = engine.state();
        if (state == BackupEngine::Completed) {
            const QStringList failed = engine.failedFiles();
            if (!failed.isEmpty()) {
                ++g_failures;
                g_out << "  FAIL 有文件复制失败：" << failed.join(", ") << "\n";
            }
            return true;
        }
        if (state == BackupEngine::Idle)
            return false;
        QThread::msleep(5);
    }
    engine.stop();
    return false;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Cjibackup");
    app.setApplicationName("CjiSmokeTest");

    QSettings clean(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    clean.clear();

    const QString root = QDir::tempPath() + "/cji_smoke";
    QDir(root).removeRecursively();
    QDir().mkpath(root);

    const QString card = root + "/card/DCIM";
    const QString target = root + "/target";

    makeFile(card + "/100CANON/IMG_0001.JPG", QDateTime(QDate(2026, 10, 31), QTime(15, 4, 5)));
    makeFile(card + "/101CANON/IMG_0002.CR3", QDateTime(QDate(2026, 9, 1), QTime(9, 30, 0)));
    makeFile(card + "/100CANON/MOV_0001.MP4", QDateTime(QDate(2026, 1, 15), QTime(20, 0, 0)));

    SettingsManager *sm = SettingsManager::instance();
    sm->setAutoRename(false);
    sm->setSubfolderByExt(false);

    g_out << "[场景一] 年 / 月 / 日 / 后缀\n";
    sm->setFlowConfig({"year", "month", "day", "ext"});
    const bool ok1 = runEngine(card, target);
    if (!ok1) {
        ++g_failures;
        g_out << "  FAIL 备份未完成\n";
    }
    expectFile(target + "/2026/10/31/JPG/IMG_0001.JPG");
    expectFile(target + "/2026/09/01/CR3/IMG_0002.CR3");
    expectFile(target + "/2026/01/15/MP4/MOV_0001.MP4");

    g_out << "[场景二] 年 / 事件 / 月 / 后缀（日志里的例子）\n";
    const QString target2 = root + "/target2";
    sm->setFlowConfig({"year", "event:校运会", "month", "ext"});
    const bool ok2 = runEngine(card, target2);
    if (!ok2) {
        ++g_failures;
        g_out << "  FAIL 备份未完成\n";
    }
    expectFile(target2 + "/2026/校运会/10/JPG/IMG_0001.JPG");
    expectFile(target2 + "/2026/校运会/09/CR3/IMG_0002.CR3");

    g_out << "[场景三] 序号节点参与文件名编号\n";
    const QString target3 = root + "/target3";
    sm->setFlowConfig({"year", "seq"});
    const bool ok3 = runEngine(card, target3);
    if (!ok3) {
        ++g_failures;
        g_out << "  FAIL 备份未完成\n";
    }
    QDir yearDir(target3 + "/2026");
    const QStringList produced = yearDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    g_out << "  生成：" << produced.join(", ") << "\n";

    const QRegularExpression numberedPattern(QStringLiteral("^\\d{3}_"));
    int numbered = 0;
    for (const QString &name : produced) {
        if (numberedPattern.match(name).hasMatch())
            ++numbered;
    }
    if (numbered == 3) {
        g_out << "  OK   3 个文件都带序号前缀\n";
    } else {
        ++g_failures;
        g_out << "  FAIL 带序号前缀的文件数量 = " << numbered << "（期望 3）\n";
    }

    /* ---------------- v1.1.0：相机型号分文件夹 + 文件重命名 ---------------- */

    const QString card2 = root + "/card2/DCIM";
    const QDateTime when(QDate(2026, 10, 31), QTime(15, 4, 5));
    const QByteArray nikon = fakeTiffWithModel("NIKON", "NIKON Z 6_2");

    makeFile(card2 + "/100NIKON/A.NEF", when, nikon);
    makeFile(card2 + "/100NIKON/B.NEF", when, nikon);
    makeFile(card2 + "/100NIKON/C.MP4", when, "video");

    sm->setAutoRename(true);
    sm->setRenamePrefix("IMG");
    sm->setRenameDigits(4);
    sm->setRenameCustom("婚礼");

    g_out << "[场景四] 相机型号分文件夹 + 前缀/序号/自定义内容\n";
    const QString readModel = ExifScanner::readCameraModel(card2 + "/100NIKON/A.NEF");
    if (readModel == QStringLiteral("Nikon Z 6II")) {
        g_out << "  OK   EXIF 型号规范化：" << readModel << "\n";
    } else {
        ++g_failures;
        g_out << "  FAIL EXIF 型号 = [" << readModel << "]，期望 Nikon Z 6II\n";
    }
    const QString target4 = root + "/target4";
    sm->setFlowConfig({"year", "month", "day", "camera", "ext"});
    const bool ok4 = runEngine(card2, target4);
    if (!ok4) {
        ++g_failures;
        g_out << "  FAIL 备份未完成\n";
    }
    expectFile(target4 + "/2026/10/31/Nikon Z 6II/NEF/IMG0001婚礼.NEF");
    expectFile(target4 + "/2026/10/31/Nikon Z 6II/NEF/IMG0002婚礼.NEF");
    // 视频读不到型号 -> 未知相机1，序号在自己的文件夹里从 1 重新开始
    expectFile(target4 + "/2026/10/31/未知相机1/MP4/IMG0001婚礼.MP4");

    g_out << "[场景五] 同一张卡两台相机：型号各自成文件夹，序号分别从 1 开始\n";
    makeFile(card2 + "/101SONY/D.ARW", when, fakeTiffWithModel("SONY", "SONY A7M4"));
    const QString target5 = root + "/target5";
    const bool ok5 = runEngine(card2, target5);
    if (!ok5) {
        ++g_failures;
        g_out << "  FAIL 备份未完成\n";
    }
    expectFile(target5 + "/2026/10/31/Sony A7M4/ARW/IMG0001婚礼.ARW");
    expectFile(target5 + "/2026/10/31/Nikon Z 6II/NEF/IMG0001婚礼.NEF");
    expectFile(target5 + "/2026/10/31/Nikon Z 6II/NEF/IMG0002婚礼.NEF");
    expectFile(target5 + "/2026/10/31/未知相机1/MP4/IMG0001婚礼.MP4");

    {
        QDirIterator files(target5, QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext())
            g_out << "  实际生成：" << QDir::toNativeSeparators(files.next()) << "\n";
    }

    clean.clear();
    QDir(root).removeRecursively();

    g_out << "\n";
    if (g_failures == 0) {
        g_out << "全部通过\n";
        return 0;
    }
    g_out << "失败 " << g_failures << " 项\n";
    return 1;
}
