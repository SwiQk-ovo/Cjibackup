#include "winutils.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QWidget>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shellapi.h>
#  include <shlobj.h>
#  include <dwmapi.h>
#endif

namespace WinUtils {

QString driveRoot(const QString &path)
{
    const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (native.size() >= 2 && native.at(1) == QLatin1Char(':'))
        return native.left(2) + QStringLiteral("\\");
    return QString();
}

bool isRemovableDrive(const QString &path)
{
#ifdef Q_OS_WIN
    const QString root = driveRoot(path);
    if (root.isEmpty())
        return false;
    const std::wstring native = QDir::toNativeSeparators(root).toStdWString();
    const UINT type = GetDriveTypeW(native.c_str());
    return type == DRIVE_REMOVABLE || type == DRIVE_REMOTE;
#else
    Q_UNUSED(path)
    return false;
#endif
}

QString driveDescription(const QString &path)
{
    const QString root = driveRoot(path);
    if (root.isEmpty())
        return QStringLiteral("未知位置");

#ifdef Q_OS_WIN
    const std::wstring native = QDir::toNativeSeparators(root).toStdWString();
    QString kind;
    switch (GetDriveTypeW(native.c_str())) {
    case DRIVE_REMOVABLE: kind = QStringLiteral("可移动磁盘"); break;
    case DRIVE_FIXED:     kind = QStringLiteral("本地磁盘"); break;
    case DRIVE_REMOTE:    kind = QStringLiteral("网络磁盘"); break;
    case DRIVE_CDROM:     kind = QStringLiteral("光驱"); break;
    case DRIVE_RAMDISK:   kind = QStringLiteral("内存盘"); break;
    default:              kind = QStringLiteral("未知类型"); break;
    }
    return QStringLiteral("%1 %2").arg(root, kind);
#else
    return root;
#endif
}

int openFormatDialog(const QString &path, QWidget *owner)
{
#ifdef Q_OS_WIN
    const QString root = driveRoot(path);
    if (root.isEmpty())
        return -99;

    const uint type = GetDriveTypeW(QDir::toNativeSeparators(root).toStdWString().c_str());
    if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED)
        return -99;

    const int index = root.at(0).toUpper().unicode() - 'A';
    if (index < 0 || index > 25)
        return -99;

    const HWND hwnd = owner ? reinterpret_cast<HWND>(owner->winId()) : nullptr;
#ifndef SHFMT_OPT_QUICK
#  define SHFMT_OPT_QUICK 0x0000
#endif
    const DWORD result = SHFormatDrive(hwnd, UINT(index), SHFMT_ID_DEFAULT, SHFMT_OPT_QUICK);

    if (result == SHFMT_ERROR || result == SHFMT_CANCEL || result == SHFMT_NOFORMAT)
        return -1;
    return 0;
#else
    Q_UNUSED(path)
    Q_UNUSED(owner)
    return -99;
#endif
}

bool revealInExplorer(const QString &path)
{
    const QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    return QProcess::startDetached(QStringLiteral("explorer.exe"), {native});
}

void applyDarkTitleBar(QWidget *window)
{
#ifdef Q_OS_WIN
    if (!window)
        return;

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;

    // Win10 20H1+ 用属性 20，早期版本用 19
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
#else
    Q_UNUSED(window)
#endif
}

} // namespace WinUtils
