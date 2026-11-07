#ifndef CJI_VERSION_H
#define CJI_VERSION_H

#include <QString>

namespace Cji {

// 版本号只在这里改，安装包 / 关于页 / 更新检查都读这里
inline constexpr int Major = 1;
inline constexpr int Minor = 1;
inline constexpr int Patch = 2;
inline constexpr bool TestBuild = false;

inline QString versionNumber()
{
    return QStringLiteral("1.1.2");
}

inline QString versionLabel()
{
    return QStringLiteral("正式版 v1.1.2");
}

} // namespace Cji

#endif // CJI_VERSION_H
