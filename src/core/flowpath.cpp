#include "flowpath.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace FlowPath {

namespace {

QString entryText(const Item &item)
{
    const QString key = keyName(item.key);
    if (item.key == Key::Event || item.key == Key::Custom)
        return key + QLatin1Char(':') + item.text;
    return key;
}

bool decodeGroup(const QString &payload, QString *sep, QStringList *entries)
{
    if (!sep || !entries)
        return false;

    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject object = doc.object();
    QString value = object.value(QStringLiteral("sep")).toString();
    if (value.isEmpty())
        value = QStringLiteral("-");

    QStringList list;
    for (const QJsonValue &item : object.value(QStringLiteral("items")).toArray()) {
        const QString entry = item.toString();
        if (!entry.isEmpty())
            list << entry;
    }
    if (list.isEmpty())
        return false;

    *sep = value;
    *entries = list;
    return true;
}

void appendEntries(const QStringList &entries, const QString &joinSep, int groupId,
                   QVector<Item> *items)
{
    if (!items)
        return;

    for (const QString &entry : entries) {
        const int sep = entry.indexOf(QLatin1Char(':'));
        const QString keyPart = sep < 0 ? entry : entry.left(sep);
        const QString textPart = sep < 0 ? QString() : entry.mid(sep + 1);

        // 合并节点里又套了一层合并：直接拍平，成员用外层的分隔符
        if (keyPart.trimmed().compare(QLatin1String("merge"), Qt::CaseInsensitive) == 0) {
            QString innerSep;
            QStringList innerEntries;
            if (decodeGroup(textPart, &innerSep, &innerEntries))
                appendEntries(innerEntries, joinSep, groupId, items);
            continue;
        }

        if (keyPart.trimmed().compare(QLatin1String("date"), Qt::CaseInsensitive) == 0) {
            // 兼容旧版配置：date = 年 / 月 / 日
            for (Key key : {Key::Year, Key::Month, Key::Day}) {
                Item item;
                item.key = key;
                item.joinSep = joinSep;
                item.groupId = groupId;
                items->append(item);
            }
            continue;
        }

        Key key;
        if (!keyFromName(keyPart, &key))
            continue;

        Item item;
        item.key = key;
        item.joinSep = joinSep;
        item.groupId = groupId;
        if (key == Key::Event || key == Key::Custom)
            item.text = textPart.trimmed();
        items->append(item);
    }
}

} // namespace

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
    int nextGroup = 0;

    for (const QString &entry : config) {
        const int sep = entry.indexOf(QLatin1Char(':'));
        const QString keyPart = sep < 0 ? entry : entry.left(sep);
        const QString textPart = sep < 0 ? QString() : entry.mid(sep + 1);

        if (keyPart.trimmed().compare(QLatin1String("merge"), Qt::CaseInsensitive) == 0) {
            QString groupSep;
            QStringList entries;
            if (!decodeGroup(textPart, &groupSep, &entries))
                continue;
            appendEntries(entries, groupSep, ++nextGroup, &items);
            continue;
        }

        appendEntries({entry}, QString(), 0, &items);
    }
    return items;
}

QStringList serialize(const QVector<Item> &items)
{
    QStringList out;
    for (int i = 0; i < items.size();) {
        const Item &item = items.at(i);
        if (item.joinSep.isEmpty() || item.groupId <= 0) {
            out << entryText(item);
            ++i;
            continue;
        }

        // 同一个合并节点里的成员是连续的，收成一条 merge: 配置
        const int groupId = item.groupId;
        const QString sep = item.joinSep;
        QJsonArray members;
        while (i < items.size() && items.at(i).groupId == groupId
               && !items.at(i).joinSep.isEmpty()) {
            members.append(entryText(items.at(i)));
            ++i;
        }

        QJsonObject object;
        object.insert(QStringLiteral("sep"), sep);
        object.insert(QStringLiteral("items"), members);
        out << QStringLiteral("merge:")
                   + QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }
    return out;
}

QString encodeGroup(const QString &separator, const QStringList &entries)
{
    QJsonArray members;
    for (const QString &entry : entries) {
        if (!entry.isEmpty())
            members.append(entry);
    }

    QJsonObject object;
    object.insert(QStringLiteral("sep"),
                  separator.isEmpty() ? QStringLiteral("-") : separator);
    object.insert(QStringLiteral("items"), members);
    return QStringLiteral("merge:")
           + QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

bool isMergeEntry(const QString &entry)
{
    const int sep = entry.indexOf(QLatin1Char(':'));
    const QString keyPart = sep < 0 ? entry : entry.left(sep);
    return keyPart.trimmed().compare(QLatin1String("merge"), Qt::CaseInsensitive) == 0;
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

namespace { QString g_separator = QStringLiteral("-"); }

QString separator() { return g_separator; }
void setSeparator(const QString &sep) { g_separator = sep.isEmpty() ? QStringLiteral("-") : sep; }

QString relativeDir(const QStringList &config, const QString &fileName, const QDateTime &dt,
                    const QString &cameraModel)
{
    const QVector<Item> items = parse(config);
    if (items.isEmpty())
        return QString();

    const QDateTime when = dt.isValid() ? dt : QDateTime::currentDateTime();
    const QString suffix = QFileInfo(fileName).suffix().toUpper();
    QStringList parts;

    auto isDateKey = [](Key key) {
        return key == Key::Year || key == Key::Month || key == Key::Day;
    };
    auto dateOf = [&](Key key) {
        switch (key) {
        case Key::Year: return when.toString(QStringLiteral("yyyy"));
        case Key::Month: return when.toString(QStringLiteral("MM"));
        default: return when.toString(QStringLiteral("dd"));
        }
    };
    auto pieceFor = [&](const Item &item) -> QString {
        switch (item.key) {
        case Key::Year:
        case Key::Month:
        case Key::Day:
            return dateOf(item.key);
        case Key::Event:
            return item.text.isEmpty() ? QStringLiteral("未命名事件") : item.text;
        case Key::Ext:
            return suffix.isEmpty() ? QStringLiteral("OTHER") : suffix;
        case Key::Custom:
            return item.text;
        case Key::Camera:
            // 型号由备份引擎逐文件读 EXIF 后传进来；预览时也会传一个示例型号
            return cameraModel.trimmed().isEmpty() ? QStringLiteral("未知相机") : cameraModel;
        case Key::Seq:
            // 序号只参与文件名编号，不生成文件夹
            return QString();
        }
        return QString();
    };

    // 年 / 月 / 日 连续出现（且没有被"合并节点"打断）时合并成一个文件夹，
    // 用用户设置的全局分隔符连接，顺序完全按用户在流程图里的排列
    const QString sep = separator();
    QString datePart;
    auto flushDate = [&]() {
        if (!datePart.isEmpty()) {
            parts.append(datePart);
            datePart.clear();
        }
    };

    // 合并节点：同组的成员用各自的 joinSep 连成一个文件夹
    int groupId = 0;
    QString groupSep;
    QStringList groupParts;
    auto flushGroup = [&]() {
        if (groupId > 0) {
            if (!groupParts.isEmpty())
                parts.append(groupParts.join(groupSep));
            groupParts.clear();
            groupSep.clear();
            groupId = 0;
        }
    };

    for (const Item &item : items) {
        if (item.groupId > 0 && !item.joinSep.isEmpty()) {
            flushDate();
            if (item.groupId != groupId) {
                flushGroup();
                groupId = item.groupId;
                groupSep = item.joinSep;
            }
            const QString part = sanitize(pieceFor(item));
            if (!part.isEmpty())
                groupParts << part;
            continue;
        }

        flushGroup();
        if (isDateKey(item.key)) {
            // 连续的 年/月/日 合并成一个目录，顺序按用户拖放的先后
            const QString piece = pieceFor(item);
            datePart = datePart.isEmpty() ? piece : (datePart + sep + piece);
            continue;
        }

        flushDate();
        const QString part = sanitize(pieceFor(item));
        if (!part.isEmpty())
            parts << part;
    }

    flushDate();
    flushGroup();
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
