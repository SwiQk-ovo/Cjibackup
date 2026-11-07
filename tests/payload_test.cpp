// 安装载荷往返测试：packer 打包 -> PayloadReader 解包 -> 比对文件
#include "payload.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>

static QTextStream g_out(stdout);
static int g_failures = 0;

static void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (argc < 2) {
        g_out << "用法: payload_test <packer.exe 路径>\n";
        return 2;
    }
    const QString packerExe = QString::fromLocal8Bit(argv[1]);

    const QString root = QDir::tempPath() + "/cji_payload_test";
    QDir(root).removeRecursively();
    QDir().mkpath(root);

    const QString sourceDir = root + "/source";
    writeFile(sourceDir + "/main.exe", QByteArray("MZ-fake-exe") + QByteArray(4096, 'x'));
    writeFile(sourceDir + "/platforms/qwindows.dll", QByteArray(2048, 'q'));
    writeFile(sourceDir + "/readme.txt", QByteArray("hello cjibackup"));

    const QString packFile = root + "/payload.pack";
    QProcess packer;
    packer.start(packerExe, {sourceDir, packFile});
    packer.waitForFinished(60000);
    if (packer.exitCode() != 0) {
        g_out << "  FAIL packer 执行失败：" << packer.readAllStandardError() << "\n";
        return 1;
    }
    g_out << "  OK   packer 生成载荷\n";

    // 造一个「安装包」：伪 exe + 载荷 + footer
    const QString fakeSetup = root + "/setup.exe";
    QFile payloadFile(packFile);
    payloadFile.open(QIODevice::ReadOnly);
    const QByteArray payloadBytes = payloadFile.readAll();
    payloadFile.close();

    {
        QFile out(fakeSetup);
        out.open(QIODevice::WriteOnly);
        out.write(QByteArray("MZ") + QByteArray(1024, 'e'));   // 假的 exe 头
        out.write(payloadBytes);
        QDataStream ds(&out);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds << quint64(payloadBytes.size());
        ds.writeRawData("CJISFX01", 8);
        out.close();
    }

    PayloadReader reader;
    QString error;
    if (!reader.open(fakeSetup, &error)) {
        g_out << "  FAIL 读取载荷失败：" << error << "\n";
        return 1;
    }
    g_out << "  OK   识别到 " << reader.count() << " 个文件\n";
    if (reader.count() != 3) {
        g_out << "  FAIL 文件数量不对，期望 3\n";
        ++g_failures;
    }

    const QString targetDir = root + "/installed";
    int extracted = 0;
    if (!reader.extractAll(targetDir,
                           [&extracted](int, const QString &) { ++extracted; }, &error)) {
        g_out << "  FAIL 解包失败：" << error << "\n";
        return 1;
    }
    g_out << "  OK   解包 " << extracted << " 个文件\n";

    const QList<QPair<QString, QString>> checks = {
        {sourceDir + "/main.exe", targetDir + "/main.exe"},
        {sourceDir + "/platforms/qwindows.dll", targetDir + "/platforms/qwindows.dll"},
        {sourceDir + "/readme.txt", targetDir + "/readme.txt"}};

    for (const auto &pair : checks) {
        QFile a(pair.first);
        QFile b(pair.second);
        const bool ok = b.exists() && a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly)
                        && a.readAll() == b.readAll() && b.size() == a.size();
        if (ok) {
            g_out << "  OK   " << QDir(sourceDir).relativeFilePath(pair.first) << "\n";
        } else {
            g_out << "  FAIL " << pair.second << "\n";
            ++g_failures;
        }
    }

    // 损坏的安装包应当被拒绝
    const QString broken = root + "/broken_pkg/broken.exe";
    {
        QDir().mkpath(QFileInfo(broken).absolutePath());
        QFile out(broken);
        out.open(QIODevice::WriteOnly);
        out.write(QByteArray(2048, 'z'));
        out.close();
    }
    PayloadReader brokenReader;
    if (!brokenReader.open(broken, &error)) {
        g_out << "  OK   损坏的安装包被拒绝（" << error << "）\n";
    } else {
        g_out << "  FAIL 损坏的安装包居然能打开\n";
        ++g_failures;
    }

    QDir(root).removeRecursively();

    g_out << "\n";
    if (g_failures == 0) {
        g_out << "全部通过\n";
        return 0;
    }
    g_out << "失败 " << g_failures << " 项\n";
    return 1;
}
