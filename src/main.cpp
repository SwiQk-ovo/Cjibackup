#include "core/appicon.h"
#include "core/localization.h"
#include "core/settingsmanager.h"
#include "core/soundplayer.h"
#include "core/version.h"
#include "mainwindow.h"
#include "widgets/splashwindow.h"

#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>

int main(int argc, char *argv[])
{
    // 高 DPI：不取整，直接按系统缩放渲染，避免 125% / 150% 下模糊
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    // 注意：这个 key 决定 QSettings（注册表）和单实例的名字，
    // 改名会让老用户的设置/历史全部读不到，所以即使软件已经改名成「拷备」也保持原值
    app.setApplicationName("C极备份");
    app.setOrganizationName("Cjibackup");

    // 让 Qt 自带的对话框（文件选择等）也显示中文
    Localization::installChineseTranslations(app);
    app.setApplicationVersion(Cji::versionNumber());
    app.setWindowIcon(AppIcon::applicationIcon());

    // 全局样式：消息框、浮层提示、文件选择框都用同一套配色
    {
        QFile qss(QStringLiteral(":/resources/style.qss"));
        if (qss.open(QIODevice::ReadOnly)) {
            app.setStyleSheet(QString::fromUtf8(qss.readAll()));
            qss.close();
        }
    }
    // 关掉窗口后常驻托盘，退出由托盘菜单或设置决定
    app.setQuitOnLastWindowClosed(false);

    // ---------------- 单实例 ----------------
    const QString serverName = QStringLiteral("Cjibackup-SingleInstance");
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            probe.write("show");
            probe.flush();
            probe.waitForBytesWritten(500);
            probe.disconnectFromServer();
            return 0;
        }
    }

    QLocalServer server;
    QLocalServer::removeServer(serverName);
    server.listen(serverName);

    MainWindow window;

    QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window]() {
        while (QLocalSocket *client = server.nextPendingConnection()) {
            client->disconnectFromServer();
            client->deleteLater();
        }
        window.showFromTray();
    });

    SettingsManager *sm = SettingsManager::instance();
    const bool enableSplash = sm->splashEnabled() && !sm->fastStart();

    if (enableSplash) {
        auto *splash = new SplashWindow();
        QObject::connect(splash, &SplashWindow::finished, &window,
                         [&window, splash, sm]() {
                             if (sm->splashSoundMode() == QStringLiteral("smooth"))
                                 AppSound::stop();
                             window.prepareShow();
                             splash->deleteLater();
                         });
        if (!sm->splashSoundPath().isEmpty())
            AppSound::playWav(sm->splashSoundPath());
        splash->start();
    } else {
        window.prepareShow();
    }

    return app.exec();
}
