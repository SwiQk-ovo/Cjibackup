#ifndef INSTALLDATA_H
#define INSTALLDATA_H

#include <QString>

namespace InstallData {

// 由 CMake 的 project(VERSION) 注入，安装向导显示的就是真实版本
#ifndef CJI_VERSION_STRING
#define CJI_VERSION_STRING "0.0.0"
#endif

const QString AppName = QStringLiteral("KOPAL");
const QString AppExeName = QStringLiteral("KOPAL.exe");
const QString UninstallerName = QStringLiteral("KOPALUninstall.exe");
const QString Version = QStringLiteral(CJI_VERSION_STRING);
const QString VersionLabel = QStringLiteral("正式版 v") + Version;

QString defaultInstallDir();
// 用户选了父目录时自动补上 KOPAL 子目录
QString normalizeInstallDir(const QString &chosen);

bool createShortcuts(const QString &installDir, bool desktopShortcut, QString *error);
bool removeShortcuts();
bool writeUninstallEntry(const QString &installDir);
bool removeUninstallEntry();
QString registeredInstallDir();

} // namespace InstallData

#endif // INSTALLDATA_H
