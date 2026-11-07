#ifndef INSTALLDATA_H
#define INSTALLDATA_H

#include <QString>

namespace InstallData {

const QString AppName = QStringLiteral("KOPAL");
const QString AppExeName = QStringLiteral("KOPAL.exe");
const QString UninstallerName = QStringLiteral("CjibackupUninstall.exe");
const QString Version = QStringLiteral("1.1.4");
const QString VersionLabel = QStringLiteral("正式版 v1.1.1");

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
