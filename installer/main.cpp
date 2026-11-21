#include "core/localization.h"
#include "installerwindow.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Localization::installChineseTranslations(app);
    app.setApplicationName(QStringLiteral("KOPALSetup"));
    app.setOrganizationName(QStringLiteral("Cjibackup"));

    QFile qss(QStringLiteral(":/theme.qss"));
    if (qss.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qss.close();
    }

    bool testMode = false;
    bool silent = false;
    bool exactDir = false;
    bool autoRestart = false;
    bool noShortcuts = false;
    QString installDir;

    for (const QString &arg : app.arguments()) {
        if (arg == QStringLiteral("--test"))
            testMode = true;
        else if (arg == QStringLiteral("--silent") || arg == QStringLiteral("/S"))
            silent = true;
        else if (arg == QStringLiteral("--exact"))
            exactDir = true;
        else if (arg == QStringLiteral("--auto"))
            autoRestart = true;
        else if (arg == QStringLiteral("--no-shortcuts"))
            noShortcuts = true;
        else if (arg.startsWith(QStringLiteral("--dir=")))
            installDir = arg.mid(6);
    }

    if (qEnvironmentVariableIsSet("CJI_INSTALLER_TEST"))
        testMode = true;

    InstallerWindow window(testMode);
    window.setExactDir(exactDir);
    window.setAutoRestart(autoRestart);
    window.setNoShortcuts(noShortcuts);
    if (!installDir.isEmpty())
        window.setInitialDir(installDir);

    if (silent)
        return window.runSilent() ? 0 : 1;

    window.show();
    return app.exec();
}
