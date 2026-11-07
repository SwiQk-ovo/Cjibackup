#include "flowpath.h"

#include <QFileInfo>
#include <QStringList>

namespace FlowPath {

QString keyName(Key key)
{
    switch (key) {
    case Key::Year:   return QStringLiteral("year");
    case Key::Month:  return QStringLiteral("month");
    case Key::Day:    return QStringLiteral("day");
    case Key::Event:  return QStringLiteral("event");
    case Key::Seq:    return QStringLiteral("seq");
    case Key::Ext:    return QStringLiteral("ext");
    case Key::Custom: return QStringLiteral("custom");
    case Key::Camera: return QStringLiteral("camera");
    }
    return QStringLiteral("year");
}

bool keyFromName(const QString &name, Key *key)
{
    if (!key)
        return false;

    const QString s = name.trimmed().toLower();
    if (s == QLatin1String("year"))   { *key = Key::Year;   return true; }
    if (s == QLatin1String("month"))  { *key = Key::Month;  return true; }
    if (s == QLatin1String("day"))    { *key = Key::Day;    return true; }
    if (s == QLatin1String("event"))  { *key = Key::Event;  return true; }
    if (s == QLatin1String("seq"))    { *key = Key::Seq;    return true; }
    if (s == QLatin1String("ext"))    { *key = Key::Ext;    return true; }
    if (s == QLatin1String("custom")) { *key = Key::Custom; return true; }
    if (s == QLatin1String("camera")) { *key = Key::Camera; return true; }
    return false;
}

QVector<Item> parse(const QStringList &config)
{
    QVector<Item> items;
    for (const QString &entry : config) {
        const int sep = entry.indexOf(QLatin1Char(':'));
        const QString keyPart = sep < 0 ? entry : entry.left(sep);
        const QString textPart = sep < 0 ? QString() : entry.mid(sep + 1);

        if (keyPart.trimmed().compare(QLatin1String("date"), Qt::CaseInsensitive) == 0) {
            // 兼容旧版配置：date = 年 / 月 / 日
            items.append({Key::Year, QString()});
            items.append({Key::Month, QString()});
            items.append({Key::Day, QString()});
            continue;
        }

        Key key;
        if (!keyFromName(keyPart, &key))
            continue;

        Item item;
        item.key = key;
        if (key == Key::Event || key == Key::Custom)
            item.text = textPart.trimmed();
        items.append(item);
    }
    return items;
}

QStringList serialize(const QVector<Item> &items)
{
    QStringList out;
    for (const Item &item : items) {
        const QString key = keyName(item.key);
        if (item.key == Key::Event || item.key == Key::Custom)
            out << key + QLatin1Char(':') + item.text;
        else
            out << key;
    }
    return out;
}

bool hasKey(const QStringList &config, Key key)
{
    const QVector<Item> items = parse(config);
    for (const Item &item : items) {
        if (item.key == key)
            return true;
    }
    return false;
}

QString sanitize(const QString &raw)
{
    QString s = raw;
    const QString invalid = QStringLiteral("\\/:*?\"<>|");
    for (const QChar &c : invalid)
        s.replace(c, QLatin1Char('_'));

    QString cleaned;
    cleaned.reserve(s.size());
    for (const QChar &c : s) {
        if (c.unicode() < 0x20)
            continue;
        cleaned.append(c);
    }

    s = cleaned.trimmed();
    while (s.endsWith(QLatin1Char('.')) || s.endsWith(QLatin1Char(' ')))
        s.chop(1);

    static const QStringList reservedNames = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"),
        QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"), QStringLiteral("LPT4"),
        QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"), QStringLiteral("LPT8"),
        QStringLiteral("LPT9")
    };
    if (reservedNames.contains(s.toUpper()))
        s.prepend(QLatin1Char('_'));

    return s;
}

QString relativeDir(const QStringList &config, const QString &fileName, const QDateTime &dt,
                    const QString &cameraModel)
{
    const QVector<Item> items = parse(config);
    if (items.isEmpty())
        return QString();

    const QDateTime when = dt.isValid() ? dt : QDateTime::currentDateTime();
    const QString suffix = QFileInfo(fileName).suffix().toUpper();
    QStringList parts;

    // 年月日的写法取决于此前是否已经出现过更粗的分类项：
    // 年 -> 2026、月 -> 10（前面有年）/ 2026-10、日 -> 31（前面有年月）/ 10-31 / 2026-10-31
    bool yearSeen = false;
    bool monthSeen = false;

    for (const Item &item : items) {
        QString part;
        switch (item.key) {
        case Key::Year:
            part = when.toString(QStringLiteral("yyyy"));
            yearSeen = true;
            break;
        case Key::Month:
            part = yearSeen ? when.toString(QStringLiteral("MM"))
                            : when.toString(QStringLiteral("yyyy-MM"));
            monthSeen = true;
            break;
        case Key::Day:
            if (yearSeen && monthSeen)
                part = when.toString(QStringLiteral("dd"));
            else if (yearSeen || monthSeen)
                part = when.toString(QStringLiteral("MM-dd"));
            else
                part = when.toString(QStringLiteral("yyyy-MM-dd"));
            break;
        case Key::Event:
            part = item.text.isEmpty() ? QStringLiteral("未命名事件") : item.text;
            break;
        case Key::Ext:
            part = suffix.isEmpty() ? QStringLiteral("OTHER") : suffix;
            break;
        case Key::Custom:
            part = item.text;
            break;
        case Key::Camera:
            // 型号由备份引擎逐文件读 EXIF 后传进来；预览时也会传一个示例型号
            part = cameraModel.trimmed().isEmpty() ? QStringLiteral("未知相机") : cameraModel;
            break;
        case Key::Seq:
            // 序号只参与文件名编号，不生成文件夹
            break;
        }

        part = sanitize(part);
        if (!part.isEmpty())
            parts << part;
    }

    return parts.join(QLatin1Char('/'));
}

QString buildFileName(const QStringList &config, const QString &originalName,
                      const QString &renameFormat, bool autoRename,
                      const QDateTime &dt, int seq, int digits)
{
    const QFileInfo info(originalName);
    const QString suffix = info.suffix();
    const QDateTime when = dt.isValid() ? dt : QDateTime::currentDateTime();
    const int width = qMax(3, digits > 0 ? digits : 3);
    const QString seqStr = QStringLiteral("%1").arg(seq, width, 10, QLatin1Char('0'));

    if (autoRename) {
        QString pattern = renameFormat.trimmed();
        if (pattern.isEmpty())
            pattern = QStringLiteral("{date}-{seq}");
        pattern.replace(QStringLiteral("{date}"), when.toString(QStringLiteral("yyyyMMdd")));
        pattern.replace(QStringLiteral("{seq}"), seqStr);
        pattern.replace(QStringLiteral("{brand}"), QStringLiteral("Camera"));
        pattern.replace(QStringLiteral("{ext}"), suffix.toUpper());

        QString base = sanitize(pattern);
        if (base.isEmpty())
            base = seqStr;
        return suffix.isEmpty() ? base : base + QLatin1Char('.') + suffix;
    }

    if (hasKey(config, Key::Seq))
        return seqStr + QLatin1Char('_') + originalName;

    return originalName;
}

QString preview(const QStringList &config, const QString &cameraModel)
{
    const QDateTime sample(QDate(2026, 10, 31), QTime(15, 4, 5));
    const QString dir = relativeDir(config, QStringLiteral("IMG_0001.JPG"), sample, cameraModel);
    const QString name = buildFileName(config, QStringLiteral("IMG_0001.JPG"), QString(),
                                       false, sample, 1, 3);
    return dir.isEmpty() ? name : dir + QLatin1Char('/') + name;
}

} // namespace FlowPath
