#ifndef CHANGELOG_H
#define CHANGELOG_H

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * 软件内「更新日志」的数据源。
 *
 * 维护方式（以后发版照着做）：
 *   1. src/core/version.h 里把版本号改成新版本
 *   2. src/core/changelog.cpp 的 releases() 最前面插入一条同名版本
 *   3. TODO / 已知问题 按需要改 todo()
 * 条目只有四种「类型」，界面上会渲染成不同颜色的小胶囊：
 *   新增 / 修复 / 优化 / 调整
 */
namespace Changelog {

enum class Kind { Added, Fixed, Improved, Changed };

struct Item
{
    Kind kind = Kind::Added;
    QString text;
};

struct Release
{
    QString version;            // "1.1.0"
    QString date;               // "2026-11-01"
    QVector<Item> items;
};

// 全部版本，新的在前
const QVector<Release> &releases();

// 和 version.h 里版本号对上的那一条；对不上时退回第一条
const Release &current();

// TODO / 已知问题
const QStringList &todo();

// 胶囊文字：新增 / 修复 / 优化 / 调整
QString kindName(Kind kind);

} // namespace Changelog

#endif // CHANGELOG_H
