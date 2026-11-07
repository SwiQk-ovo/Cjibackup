#ifndef CAMERANAMING_H
#define CAMERANAMING_H

#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

/**
 * 文件重命名规则：前缀 + 序号 + 自定义内容
 * 例：prefix=IMG, digits=4, custom=""  ->  IMG0001
 *     prefix=照片, digits=5, custom=婚礼 -> 照片00001婚礼
 */
struct NamingPlan
{
    QString prefix = QStringLiteral("IMG");
    int digits = 4;                 // 序号位数（不足补 0）
    QString custom;                 // 自定义后缀内容

    QString build(int sequence) const
    {
        QString number = QString::number(sequence);
        if (digits > number.size())
            number = QString(digits - number.size(), QLatin1Char('0')) + number;

        QString name = prefix + number + custom;
        // 文件名里不能出现的字符，替换掉
        name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
        name = name.trimmed();
        while (name.endsWith(QLatin1Char('.')))
            name.chop(1);
        if (name.isEmpty())
            name = QStringLiteral("IMG") + number;
        return name;
    }

    bool isValid() const { return !prefix.trimmed().isEmpty() || digits >= 0; }
};

/**
 * 相机型号 -> 目标文件夹名。
 * · 已知型号：直接用完整型号（非法字符替换成 _），同名目录自然合并
 * · 未知型号：未知相机1 / 未知相机2 …（同一台卡内稳定递增，同型号复用一个编号）
 */
class CameraFolderAllocator
{
public:
    QString folderFor(const QString &model)
    {
        const QString key = model.trimmed();
        if (key.isEmpty())
            return unknownFolder(QStringLiteral("__unnamed__"));

        if (m_assigned.contains(key))
            return m_assigned.value(key);

        QString safe = sanitize(key);
        m_assigned.insert(key, safe);
        return safe;
    }

    // 显式登记一个未知来源（同一个 tag 复用同一个编号）
    QString unknownFolder(const QString &tag)
    {
        if (m_assigned.contains(tag))
            return m_assigned.value(tag);

        const QString name = QStringLiteral("未知相机%1").arg(++m_unknownCount);
        m_assigned.insert(tag, name);
        return name;
    }

    int unknownCount() const { return m_unknownCount; }

    void reset()
    {
        m_assigned.clear();
        m_unknownCount = 0;
    }

    static QString sanitize(const QString &model)
    {
        QString s = model;
        s.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
        s.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        s = s.trimmed();
        while (s.endsWith(QLatin1Char('.')))
            s.chop(1);
        return s.isEmpty() ? QStringLiteral("未知相机") : s;
    }

private:
    QHash<QString, QString> m_assigned;
    int m_unknownCount = 0;
};

/**
 * 序号分配器：按"目标文件夹"分别计数（方案 C）。
 * 《folderKey 一般就是 相机型号 + 日期目录 的组合》每换一个文件夹，序号从 1 重新开始。
 * 例：Nikon Z 6II/2026-10-31 -> IMG0001…；Sony A7M4/2026-10-31 -> IMG0001…
 */
class SequenceAllocator
{
public:
    int next(const QString &folderKey)
    {
        const int value = m_counters.value(folderKey, 0) + 1;
        m_counters.insert(folderKey, value);
        return value;
    }

    int current(const QString &folderKey) const { return m_counters.value(folderKey, 0); }

    void reset() { m_counters.clear(); }

private:
    QHash<QString, int> m_counters;
};

#endif // CAMERANAMING_H
