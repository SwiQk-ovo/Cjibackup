#include "installdata.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <objbase.h>
#  include <shlobj.h>
#endif

namespace {

const QString kUninstallKey =
    QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Cjibackup");

#ifdef Q_OS_WIN
bool createShortcut(const QString &lnkPath, const QString &target, const QString &workDir,
                    const QString &description)
{
    const std::wstring targetNative = QDir::toNativeSeparators(target).toStdWString();
    const std::wstring workNative = QDir::toNativeSeparators(workDir).toStdWString();
    const std::wstring lnkNative = QDir::toNativeSeparators(lnkPath).toStdWString();
    const std::wstring desc = description.toStdWString();

    IShellLinkW *link = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void **>(&link));
    if (FAILED(hr) || !link)
        return false;

    link->SetPath(targetNative.c_str());
    link->SetWorkingDirectory(workNative.c_str());
    link->SetDescription(desc.c_str());
    link->SetIconLocation(targetNative.c_str(), 0);

    bool ok = false;
    IPersistFile *file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file))) && file) {
        QDir().mkpath(QFileInfo(lnkPath).absolutePath());
        ok = SUCCEEDED(file->Save(reinterpret_cast<LPCOLESTR>(lnkNative.c_str()), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}
#endif

// 改名（C极备份 → 拷备）前留下的旧快捷方式，升级安装时清掉，免得留一个打不开的图标
static void cjiInstallLog(const QString &message)
{
    QFile file(QDir::tempPath() + QStringLiteral("/cji_installer.log"));
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))
            << " [shortcut] " << message << "\n";
    }
}

void cleanupLegacyShortcuts()
{
    const QStringList legacy = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
            + QStringLiteral("/C极备份.lnk"),
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/C极备份.lnk")
    };
    for (const QString &path : legacy) {
        // 注意：QFileInfo::exists() 在这里对 .lnk 会误报 false（实测），所以直接 remove
        if (QFile::remove(path))
            cjiInstallLog(QStringLiteral("已清理旧快捷方式 %1").arg(path));
    }
}

QString desktopShortcutPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
           + QStringLiteral("/拷备.lnk");
}

QString startMenuShortcutPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/拷备.lnk");
}

} // namespace

namespace InstallData {

QString defaultInstallDir()
{
    // QDir("D:/").exists() 对不存在的盘也返回 true，必须用 QDir::drives() 过滤
    for (const QFileInfo &drive : QDir::drives()) {
        const QString root = drive.absoluteFilePath();
        if (root.startsWith(QStringLiteral("D:"), Qt::CaseInsensitive) && QFileInfo(root).isWritable())
            return root + QStringLiteral("Cjibackup");
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("Cjibackup"));
}

QString normalizeInstallDir(const QString &chosen)
{
    QString path = QDir::fromNativeSeparators(chosen.trimmed());
    while (path.endsWith(QLatin1Char('/')) && path.size() > 3)
        path.chop(1);
    if (path.isEmpty())
        return defaultInstallDir();

    const QFileInfo info(path);
    if (info.fileName().compare(QStringLiteral("Cjibackup"), Qt::CaseInsensitive) != 0)
        path += QStringLiteral("/Cjibackup");
    return path;
}

bool createShortcuts(const QString &installDir, bool desktopShortcut, QString *error)
{
    cleanupLegacyShortcuts();
#ifdef Q_OS_WIN
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        // 已经初始化过也可以继续
    }

    const QString exePath = installDir + QLatin1Char('/') + AppExeName;
    bool ok = true;

    if (desktopShortcut) {
        if (!createShortcut(desktopShortcutPath(), exePath, installDir,
                            QStringLiteral("拷备 - 相机存储卡备份"))) {
            ok = false;
            if (error)
                *error = QStringLiteral("创建桌面快捷方式失败");
        }
    }

    if (!createShortcut(startMenuShortcutPath(), exePath, installDir,
                        QStringLiteral("拷备 - 相机存储卡备份"))) {
        ok = false;
        if (error && error->isEmpty())
            *error = QStringLiteral("创建开始菜单快捷方式失败");
    }

    CoUninitialize();
    return ok;
#else
    Q_UNUSED(installDir)
    Q_UNUSED(desktopShortcut)
    Q_UNUSED(error)
    return false;
#endif
}

bool removeShortcuts()
{
    cleanupLegacyShortcuts();
    bool removed = false;
    for (const QString &path : {desktopShortcutPath(), startMenuShortcutPath()}) {
        if (QFileInfo::exists(path)) {
            QFile::remove(path);
            removed = true;
        }
    }
    return removed;
}

bool writeUninstallEntry(const QString &installDir)
{
    const QString nativeDir = QDir::toNativeSeparators(installDir);
    const QString uninstaller = nativeDir + QStringLiteral("\\") + UninstallerName;

    QSettings reg(kUninstallKey, QSettings::NativeFormat);
    reg.setValue(QStringLiteral("DisplayName"), QStringLiteral("拷备"));
    reg.setValue(QStringLiteral("DisplayVersion"), Version);
    reg.setValue(QStringLiteral("Publisher"), QStringLiteral("适诠SwiQk_"));
    reg.setValue(QStringLiteral("DisplayIcon"), nativeDir + QStringLiteral("\\") + AppExeName);
    reg.setValue(QStringLiteral("InstallLocation"), nativeDir);
    reg.setValue(QStringLiteral("UninstallString"), QStringLiteral("\"%1\"").arg(uninstaller));
    reg.setValue(QStringLiteral("QuietUninstallString"), QStringLiteral("\"%1\" /S").arg(uninstaller));
    reg.setValue(QStringLiteral("NoModify"), 1);
    reg.setValue(QStringLiteral("NoRepair"), 1);
    reg.setValue(QStringLiteral("EstimatedSize"), 60000);
    reg.sync();
    return reg.status() == QSettings::NoError;
}

bool removeUninstallEntry()
{
    QSettings reg(kUninstallKey, QSettings::NativeFormat);
    reg.remove(QString());
    reg.sync();
    QSettings("HKEY_CURRENT_USER\\Software", QSettings::NativeFormat).remove("Cjibackup");
    return true;
}

QString registeredInstallDir()
{
    QSettings reg(kUninstallKey, QSettings::NativeFormat);
    return reg.value(QStringLiteral("InstallLocation")).toString();
}

} // namespace InstallData
