#ifndef APPDIRS_H
#define APPDIRS_H

#include <QString>

namespace Cji {

/** 用户数据目录：~/.kopal */
QString dataDir();

/**
 * 把老版本的数据目录（~/.cjibackup）搬过来。
 * 只在「新目录还不存在、老目录存在」的时候搬一次，搬失败就保留老目录，不影响启动。
 */
void migrateLegacyDataDir();

} // namespace Cji

#endif // APPDIRS_H
