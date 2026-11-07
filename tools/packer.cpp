// 打包工具：把目录打成安装用的载荷
// 用法:
//   packer <源目录> <输出.pack>            压缩载荷（安装器用，qCompress）
//   packer --stored <源目录> <输出.boot>   原样载荷（引导程序用，不压缩）
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>

namespace {

QStringList collectFiles(const QString &root)
{
    QStringList files;
    QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files << QDir(root).relativeFilePath(it.filePath());
    }
    files.sort();
    return files;
}

quint64 totalSize(const QString &root, const QStringList &files)
{
    quint64 total = 0;
    for (const QString &rel : files)
        total += QFileInfo(QDir(root).filePath(rel)).size();
    return total;
}

int packCompressed(const QString &root, const QString &target, QTextStream &out)
{
    const QStringList files = collectFiles(root);
    const quint64 totalRaw = totalSize(root, files);

    QFile output(target);
    if (!output.open(QIODevice::WriteOnly)) {
        out << "无法写入: " << target << "\n";
        return 3;
    }

    QDataStream ds(&output);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setVersion(QDataStream::Qt_6_0);

    ds.writeRawData("CJIPK002", 8);
    ds << quint32(files.size()) << totalRaw;

    for (const QString &rel : files) {
        QFile file(QDir(root).filePath(rel));
        if (!file.open(QIODevice::ReadOnly)) {
            out << "读取失败: " << rel << "\n";
            return 4;
        }
        const QByteArray raw = file.readAll();
        file.close();

        const QByteArray compressed = qCompress(raw, 6);
        const QByteArray pathBytes = rel.toUtf8();

        ds << quint16(pathBytes.size());
        ds.writeRawData(pathBytes.constData(), pathBytes.size());
        ds << quint64(raw.size()) << quint32(compressed.size());
        ds.writeRawData(compressed.constData(), compressed.size());
    }

    output.close();

    out << "压缩载荷完成: " << QDir::toNativeSeparators(target) << "\n"
        << "  文件数: " << files.size() << "\n"
        << "  原始大小: " << QString::number(double(totalRaw) / 1024.0 / 1024.0, 'f', 2) << " MB\n"
        << "  载荷大小: "
        << QString::number(double(QFileInfo(target).size()) / 1024.0 / 1024.0, 'f', 2) << " MB\n";
    return 0;
}

int packStored(const QString &root, const QString &target, QTextStream &out)
{
    const QStringList files = collectFiles(root);
    const quint64 totalRaw = totalSize(root, files);

    QFile output(target);
    if (!output.open(QIODevice::WriteOnly)) {
        out << "无法写入: " << target << "\n";
        return 3;
    }

    QDataStream ds(&output);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds.writeRawData("CJIBT001", 8);
    ds << quint32(files.size());

    for (const QString &rel : files) {
        QFile file(QDir(root).filePath(rel));
        if (!file.open(QIODevice::ReadOnly)) {
            out << "读取失败: " << rel << "\n";
            return 4;
        }
        const QByteArray raw = file.readAll();
        file.close();

        const QByteArray pathBytes = rel.toUtf8();
        ds << quint16(pathBytes.size());
        ds.writeRawData(pathBytes.constData(), pathBytes.size());
        ds << quint64(raw.size());
        ds.writeRawData(raw.constData(), raw.size());
    }

    output.close();

    out << "引导载荷完成: " << QDir::toNativeSeparators(target) << "\n"
        << "  文件数: " << files.size() << "\n"
        << "  原始大小: " << QString::number(double(totalRaw) / 1024.0 / 1024.0, 'f', 2) << " MB\n"
        << "  载荷大小: "
        << QString::number(double(QFileInfo(target).size()) / 1024.0 / 1024.0, 'f', 2) << " MB\n";
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    bool stored = false;
    QStringList args;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--stored")) {
            stored = true;
            continue;
        }
        args << arg;
    }

    if (args.size() < 2) {
        out << "用法: packer <源目录> <输出.pack>\n"
            << "      packer --stored <源目录> <输出.boot>\n";
        return 2;
    }

    const QString root = args.at(0);
    const QString target = args.at(1);

    if (!QFileInfo(root).isDir()) {
        out << "源目录不存在: " << root << "\n";
        return 2;
    }

    return stored ? packStored(root, target, out) : packCompressed(root, target, out);
}
