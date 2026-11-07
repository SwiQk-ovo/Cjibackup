#include "core/localization.h"
#include "installdata.h"
#include "widgets/darkdialog.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

bool looksLikeInstallDir(const QString &dir)
{
    if (!QFileInfo(dir).isDir())
        return false;
    const QFileInfo app(dir + QLatin1Char('/') + InstallData::AppExeName);
    const QFileInfo uninstaller(dir + QLatin1Char('/') + InstallData::UninstallerName);
    return app.exists() || uninstaller.exists();
}

// 删除除自身之外的所有内容（自身正在运行，删不掉，交给批处理收尾）
void removeExceptSelf(const QString &dir, const QString &selfPath)
{
    const QString self = QFileInfo(selfPath).absoluteFilePath();

    QStringList dirs;
    QDirIterator it(dir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.isDir())
            dirs << info.absoluteFilePath();
        else if (info.absoluteFilePath() != self)
            QFile::remove(info.absoluteFilePath());
    }

    // 从最深的目录开始删
    std::sort(dirs.begin(), dirs.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    for (const QString &d : dirs)
        QDir().rmdir(d);
}

// 写一个 ANSI 编码的收尾批处理：等本进程退出后把安装目录整个删掉
bool scheduleDirectoryRemoval(const QString &dir)
{
    const QString batPath =
        QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("cji_uninstall_cleanup.bat"));

    const QString content = QStringLiteral("@echo off\r\n"
                                           "ping -n 3 127.0.0.1 >nul\r\n"
                                           "rd /s /q \"%1\"\r\n"
                                           "del /q \"%%~f0\"\r\n")
                                .arg(QDir::toNativeSeparators(dir));

#ifdef Q_OS_WIN
    const int size = WideCharToMultiByte(CP_ACP, 0,
                                         reinterpret_cast<LPCWSTR>(content.utf16()), -1,
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return false;
    QByteArray ansi(size, 0);
    WideCharToMultiByte(CP_ACP, 0, reinterpret_cast<LPCWSTR>(content.utf16()), -1,
                        ansi.data(), size, nullptr, nullptr);
#else
    const QByteArray ansi = content.toLocal8Bit();
#endif

    QFile bat(batPath);
    if (!bat.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    bat.write(ansi);
    bat.close();

    return QProcess::startDetached(QStringLiteral("cmd.exe"),
                                   {QStringLiteral("/c"), batPath});
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Localization::installChineseTranslations(app);
    app.setApplicationName(QStringLiteral("CjibackupUninstall"));
    app.setOrganizationName(QStringLiteral("Cjibackup"));

    QFile qss(QStringLiteral(":/theme.qss"));
    if (qss.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qss.close();
    }

    bool silent = false;
    QString installDir;
    for (const QString &arg : app.arguments()) {
        if (arg == QStringLiteral("/S") || arg == QStringLiteral("--silent"))
            silent = true;
        else if (arg.startsWith(QStringLiteral("--dir=")))
            installDir = arg.mid(6);
    }

    const QString selfPath = QCoreApplication::applicationFilePath();
    if (installDir.isEmpty())
        installDir = InstallData::registeredInstallDir();
    if (installDir.isEmpty())
        installDir = QFileInfo(selfPath).absolutePath();
    installDir = QDir::fromNativeSeparators(installDir);

    if (!silent) {
        const bool confirmed = DarkDialog::question(
            nullptr, QStringLiteral("卸载 KOPAL"),
            QString("确定要卸载 KOPAL 吗？\n\n安装位置：%1\n\n"
                    "你的备份照片、软件设置和历史记录都不会被删除。")
                .arg(QDir::toNativeSeparators(installDir)),
            QStringLiteral("卸载"), QStringLiteral("取消"));
        if (!confirmed)
            return 0;
    }

    InstallData::removeShortcuts();
    InstallData::removeUninstallEntry();

    bool removed = false;
    if (looksLikeInstallDir(installDir)) {
        removeExceptSelf(installDir, selfPath);
        removed = scheduleDirectoryRemoval(installDir);
    }

    if (!silent) {
        DarkDialog::information(
            nullptr, QStringLiteral("卸载完成"),
            removed ? QStringLiteral("KOPAL 已卸载，感谢使用。")
                    : QStringLiteral("KOPAL 的快捷方式和注册信息已清除，安装目录请手动删除。"),
            QStringLiteral("完成"));
    }
    return 0;
}
