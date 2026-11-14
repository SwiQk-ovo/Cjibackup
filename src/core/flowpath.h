#ifndef FLOWPATH_H
#define FLOWPATH_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QDateTime>

/**
 * 流程图目录构建工具
 *
 * 流程配置（QStringList）里的每一项都是一个"分类项"，按先后顺序构成目录层级：
 *   "year"          年    -> 2026
 *   "month"         月    -> 10
 *   "day"           日    -> 31
 *   "event:校运会"  事件  -> 校运会
 *   "seq"           序号  -> 只参与文件名编号，不生成文件夹
 *   "ext"           后缀  -> JPG / RAW / MP4
 *   "custom:旅行"   自定义-> 旅行
 *   "camera"        相机型号 -> Nikon Z 6II（读不到型号时用"未知相机N"）
 *
 * 连续的 年/月/日 会自动合并成一个文件夹（用 FlowPath::separator() 连接）。
 * 2.0.3 起支持"合并节点"：把几个分类项包成一组，组成员用各自的 joinSep 连接成一个文件夹，
 * 配置里写成一条：
 *   merge:{"items":["year","month","day"],"sep":"."}
 * 兼容旧键 "date"（等价于 年/月/日 三级），未知键会被忽略。
 */
namespace FlowPath {

enum class Key { Year, Month, Day, Event, Seq, Ext, Custom, Camera };

struct Item
{
    Key key = Key::Year;
    QString text;      // 事件名 / 自定义文本
    QString joinSep;   // 非空 = 属于某个"合并节点"，同组成员用这个分隔符连成一个文件夹
    int groupId = 0;   // >0 表示合并节点编号；同一个合并节点里的成员编号相同
};

// 配置字符串 <-> 结构体
QVector<Item> parse(const QStringList &config);
QStringList serialize(const QVector<Item> &items);

// 合并节点：把 entries（如 "year" / "event:校运会"）包成一条配置
QString encodeGroup(const QString &separator, const QStringList &entries);
bool isMergeEntry(const QString &entry);

// 键名转换
QString keyName(Key key);
bool keyFromName(const QString &name, Key *key);

bool hasKey(const QStringList &config, Key key);

// 年月日合并成一个文件夹时用的分隔符（默认 "-"）
QString separator();
void setSeparator(const QString &sep);

// 生成相对目录（不含目标根目录），例如 "2026/10/Nikon Z 6II/JPG"；可能为空字符串
// cameraModel 只在使用到 "camera" 节点时生效，传空则回退成"未知相机"
QString relativeDir(const QStringList &config, const QString &fileName, const QDateTime &dt,
                    const QString &cameraModel = QString());

// 生成目标文件名
QString buildFileName(const QStringList &config, const QString &originalName,
                      const QString &renameFormat, bool autoRename,
                      const QDateTime &dt, int seq, int digits);

// 目录预览（示例文件）；cameraModel 用来在预览里显示相机型号
QString preview(const QStringList &config, const QString &cameraModel = QString());

// 去掉 Windows 路径非法字符
QString sanitize(const QString &raw);

} // namespace FlowPath

#endif // FLOWPATH_H
