#include "exifscanner.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

quint16 readU16(const QByteArray &data, int offset, bool little)
{
    if (offset + 2 > data.size())
        return 0;
    const uchar *p = reinterpret_cast<const uchar *>(data.constData()) + offset;
    if (little)
        return quint16(p[0] | (p[1] << 8));
    return quint16((p[0] << 8) | p[1]);
}

quint32 readU32(const QByteArray &data, int offset, bool little)
{
    if (offset + 4 > data.size())
        return 0;
    const uchar *p = reinterpret_cast<const uchar *>(data.constData()) + offset;
    if (little)
        return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

// 从一个 TIFF 结构的块里读 Make / Model（原样返回，规范化交给 normalizeModel）
QString readModelFromTiff(const QByteArray &data, int tiffStart, QString *makeOut)
{
    if (tiffStart + 8 > data.size())
        return QString();

    const bool little = data.mid(tiffStart, 2) == QByteArray("II");
    if (!little && data.mid(tiffStart, 2) != QByteArray("MM"))
        return QString();

    const quint16 magic = readU16(data, tiffStart + 2, little);
    if (magic != 42)
        return QString();

    const quint32 ifdOffset = readU32(data, tiffStart + 4, little);
    const int ifd = tiffStart + int(ifdOffset);
    if (ifd <= 0 || ifd + 2 > data.size())
        return QString();

    const quint16 count = readU16(data, ifd, little);
    if (count == 0 || count > 512)
        return QString();

    QString make;
    QString model;

    for (int i = 0; i < count; ++i) {
        const int entry = ifd + 2 + i * 12;
        if (entry + 12 > data.size())
            break;

        const quint16 tag = readU16(data, entry, little);
        if (tag != 0x010F && tag != 0x0110)
            continue;

        const quint16 type = readU16(data, entry + 2, little);
        const quint32 num = readU32(data, entry + 4, little);
        if (type != 2 || num == 0 || num > 256)   // 只要 ASCII
            continue;

        int valueOffset = entry + 8;
        if (num > 4)
            valueOffset = tiffStart + int(readU32(data, entry + 8, little));
        if (valueOffset < 0 || valueOffset + int(num) > data.size())
            continue;

        const QString value = QString::fromLatin1(data.constData() + valueOffset, int(num))
                                  .trimmed()
                                  .remove(QChar('\0'))
                                  .trimmed();
        if (tag == 0x010F)
            make = value;
        else
            model = value;
    }

    if (model.isEmpty())
        return QString();

    if (makeOut)
        *makeOut = make;

    return model;
}

namespace {

struct BrandRule
{
    const char *match;
    const char *name;
};

const BrandRule kBrands[] = {
    {"NIKON", "Nikon"}, {"SONY", "Sony"}, {"CANON", "Canon"},
    {"FUJIFILM", "Fujifilm"}, {"OLYMPUS", "Olympus"}, {"OM Digital", "OM System"},
    {"PANASONIC", "Panasonic"}, {"RICOH IMAGING", "Ricoh"}, {"LEICA", "Leica"},
    {"PENTAX", "Pentax"}, {"SIGMA", "Sigma"}, {"Apple", "Apple"}, {"HASSELBLAD", "Hasselblad"}};

QString brandNameOf(const QString &text)
{
    const QString s = text.simplified();
    for (const BrandRule &rule : kBrands) {
        if (s.startsWith(QLatin1String(rule.match), Qt::CaseInsensitive))
            return QString::fromLatin1(rule.name);
    }
    return QString();
}

// 不少机器把代数写在型号里：NIKON Z 6_2 / Z 7_2 -> Nikon Z 6II / Z 7II
QString romanizeModelSuffix(const QString &model)
{
    static const QRegularExpression suffix(QStringLiteral("_([2345])\\s*$"));
    const QRegularExpressionMatch match = suffix.match(model);
    if (!match.hasMatch())
        return model;

    static const char *romans[] = {"", "", "II", "III", "IV", "V"};
    const int digits = match.captured(1).toInt();
    if (digits < 2 || digits > 5)
        return model;

    return model.left(match.capturedStart()) + QString::fromLatin1(romans[digits]);
}

} // namespace

// 型号规范化：统一品牌写法（NIKON -> Nikon）、补上被 Model 吞掉的品牌、
// 再把 _2 / _3 这种世代后缀换成 II / III。
QString normalizeModel(const QString &make, const QString &model)
{
    QString core = model.simplified();
    const QString makeText = make.simplified();

    if (!makeText.isEmpty() && core.startsWith(makeText, Qt::CaseInsensitive))
        core = core.mid(makeText.size()).trimmed();

    // 型号里自带的品牌写法先统一
    for (const BrandRule &rule : kBrands) {
        if (core.startsWith(QLatin1String(rule.match), Qt::CaseInsensitive)) {
            core = QString::fromLatin1(rule.name) + core.mid(int(qstrlen(rule.match)));
            break;
        }
    }

    // 型号里没有品牌时，用 Make 补一个
    const QString brand = brandNameOf(makeText);
    if (!brand.isEmpty() && brandNameOf(core).isEmpty())
        core = brand + QLatin1Char(' ') + core;

    return romanizeModelSuffix(core.simplified());
}

} // namespace

QString ExifScanner::readCameraModel(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    const QByteArray data = file.read(256 * 1024);
    if (data.size() < 32)
        return QString();

    QString make;
    QString model;

    // TIFF 结构的 RAW：II*\0 / MM\0*
    if (data.startsWith("II") || data.startsWith("MM"))
        model = readModelFromTiff(data, 0, &make);

    // JPEG：在 APP1 里找 Exif
    if (model.isEmpty()) {
        make.clear();
        const int exifAt = data.indexOf("Exif\0\0");
        if (exifAt >= 0)
            model = readModelFromTiff(data, exifAt + 6, &make);
    }

    if (model.isEmpty())
        return QString();

    return normalizeModel(make, model);
}

QVector<CameraInfo> ExifScanner::scanDirectory(const QString &root, int maxFiles)
{
    static const QSet<QString> exts{
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("cr2"), QStringLiteral("cr3"), QStringLiteral("nef"), QStringLiteral("arw"),
        QStringLiteral("dng"), QStringLiteral("orf"), QStringLiteral("rw2"), QStringLiteral("pef"),
        QStringLiteral("raf")};

    QHash<QString, int> counter;
    int scanned = 0;

    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && scanned < maxFiles) {
        const QString path = it.next();
        const QString ext = QFileInfo(path).suffix().toLower();
        if (!exts.contains(ext))
            continue;

        const QString model = readCameraModel(path);
        if (model.isEmpty())
            continue;

        counter[model] += 1;
        ++scanned;
    }

    QVector<CameraInfo> result;
    result.reserve(counter.size());
    for (auto it = counter.constBegin(); it != counter.constEnd(); ++it) {
        CameraInfo info;
        info.model = it.key();
        info.files = it.value();
        result.append(info);
    }

    std::sort(result.begin(), result.end(), [](const CameraInfo &a, const CameraInfo &b) {
        if (a.files != b.files)
            return a.files > b.files;
        return a.model < b.model;
    });

    return result;
}

QString ExifScanner::folderSafeModel(const QString &model)
{
    QString s = model;
    s.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    s.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    s = s.trimmed();
    while (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    if (s.isEmpty())
        s = QStringLiteral("未知相机");
    return s;
}
