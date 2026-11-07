#ifndef INSTALLDATA_H
#define INSTALLDATA_H

#include <QString>

namespace InstallData {

const QString AppName = QStringLiteral("KOPAL");
const QString AppExeName = QStringLiteral("KOPAL.exe");
const QString UninstallerName = QStringLiteral("CjibackupUninstall.exe");
const QString Version = QStringLiteral("1.1.5");
// 版本号只改上面那行 Version，标题跟着走，避免两处对不上
const QString VersionLabel = QStringLiteral("正式版 v") + Version;

QString defaultInstallDir();
// 用户选了父目录时自动补上 Cjibackup 子目录
QString normalizeInstallDir(const QString &chosen);

bool createShortcuts(const QString &installDir, bool desktopShortcut, QString *error);
bool removeShortcuts();
bool writeUninstallEntry(const QString &installDir);
bool removeUninstallEntry();
QString registeredInstallDir();

} // namespace InstallData

#endif // INSTALLDATA_H
