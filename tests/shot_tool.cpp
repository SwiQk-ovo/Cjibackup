// 离屏截图工具：把各个页面渲染成 PNG，用于文档 / 预览
#include "mainwindow.h"
#include "widgets/splashwindow.h"
#include "widgets/typewritertooltip.h"
#include "core/appicon.h"
#include "widgets/flowcanvas.h"
#include "core/localization.h"
#include "core/settingsmanager.h"
#include "pages/backup_page.h"
#include "pages/settings_page.h"
#include "widgets/changelogdialog.h"
#include "widgets/darkdialog.h"
#include "widgets/filepickerdialog.h"
#include "widgets/toast.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QFileDialog>
#include <QGraphicsView>
#include <QGroupBox>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>

static void wait(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
}

static void saveWidget(QWidget *widget, const QString &path)
{
    widget->update();
    wait(60);
    const QPixmap pixmap = widget->grab();
    if (pixmap.save(path))
        QTextStream(stdout) << "saved " << path << "\n";
    else
        QTextStream(stdout) << "FAILED " << path << "\n";
}

// 点一下折叠区的标题行（标题行是 section 的第一个子控件）
static bool clickSection(QWidget *root, const QString &objectName)
{
    QWidget *section = root->findChild<QWidget *>(objectName);
    if (!section)
        return false;
    QWidget *header = section->childAt(10, 8);
    if (!header)
        return false;

    const QPoint local(10, 8);
    const QPointF global = header->mapToGlobal(local);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(local), global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(local), global,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(header, &press);
    QApplication::sendEvent(header, &release);
    return true;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName("Cjibackup");
    app.setApplicationName("CjiShotTool");
    Localization::installChineseTranslations(app);

    // 用法：shot_tool.exe [输出目录] [--splash]
    //   默认不重放启动动画（那 13 套已经定稿，跑一遍要二十多秒）
    //   要看动画截图时才加 --splash
    QString outDir = QDir::currentPath();
    bool withSplash = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--splash") || arg == QStringLiteral("--full"))
            withSplash = true;
        else if (!arg.startsWith(QStringLiteral("--")))
            outDir = arg;
    }
    QDir().mkpath(outDir);

    QFile qss(":/resources/style.qss");
    if (qss.open(QFile::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qss.close();
    }

    // 截图工具不联网，避免自动更新弹窗打断
    SettingsManager::instance()->setAutoCheckUpdates(false);
    SettingsManager::instance()->setReminderEnabled(false);

    MainWindow window;
    window.resize(1000, 680);
    window.show();
    wait(700);

    saveWidget(&window, outDir + "/window_backup.png");

    // 把最后一个节点往下挪一点，检查错位连线的走向（老版本这里会画成锯齿）
    if (auto *flowCanvas = window.findChild<FlowCanvas *>()) {
        if (auto *flowScene = flowCanvas->findChild<FlowScene *>()) {
            FlowNodeItem *rightmost = nullptr;
            for (QGraphicsItem *item : flowScene->items()) {
                auto *node = dynamic_cast<FlowNodeItem *>(item->toGraphicsObject());
                if (!node)
                    continue;
                if (!rightmost || node->pos().x() > rightmost->pos().x())
                    rightmost = node;
            }
            if (rightmost) {
                rightmost->moveBy(0, 34);
                flowScene->update();
                wait(300);
                saveWidget(&window, outDir + "/window_backup_offset.png");
                rightmost->moveBy(0, -34);
                flowScene->update();
                wait(200);
            }

            // 真模拟拖拽（连拖 4 次），检查会不会卡死/留残影
            if (auto *view = flowCanvas->findChild<QGraphicsView *>()) {
                if (rightmost) {
                    const QPointF center = rightmost->boundingRect().center();
                    const QPoint startPos = view->mapFromScene(rightmost->mapToScene(center));
                    for (int round = 0; round < 4; ++round) {
                        const QPoint base = startPos + QPoint(round * 5, 0);
                        QMouseEvent press(QEvent::MouseButtonPress, base,
                                          view->viewport()->mapToGlobal(base), Qt::LeftButton,
                                          Qt::LeftButton, Qt::NoModifier);
                        QApplication::sendEvent(view->viewport(), &press);
                        for (int step = 1; step <= 14; ++step) {
                            const QPoint p = base + QPoint(0, step * 3);
                            QMouseEvent move(QEvent::MouseMove, p,
                                             view->viewport()->mapToGlobal(p), Qt::NoButton,
                                             Qt::LeftButton, Qt::NoModifier);
                            QApplication::sendEvent(view->viewport(), &move);
                            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
                            QThread::msleep(16);
                        }
                        saveWidget(&window, QString("%1/window_backup_drag%2.png").arg(outDir).arg(round));
                        const QPoint endPos = base + QPoint(0, 42);
                        QMouseEvent release(QEvent::MouseButtonRelease, endPos,
                                            view->viewport()->mapToGlobal(endPos), Qt::LeftButton,
                                            Qt::NoButton, Qt::NoModifier);
                        QApplication::sendEvent(view->viewport(), &release);
                        wait(300);
                    }
                    QTextStream(stdout) << "drag rounds finished\n";
                }
            }
        }
    }

    // 导出程序图标（打包 .ico 用）
    for (int size : {256, 64, 48, 32, 16}) {
        const QPixmap logo = AppIcon::logoPixmap(size);
        logo.save(QString("%1/app_icon_%2.png").arg(outDir).arg(size));
    }

    // v1.1.0：相机型号节点 + 文件重命名胶囊区
    if (auto *flowCanvas = window.findChild<FlowCanvas *>()) {
        flowCanvas->setCameraModelHint(QStringLiteral("Nikon Z 6II"));
        flowCanvas->setFlowConfig({"year", "month", "day", "camera", "ext"});
        wait(420);
        saveWidget(&window, outDir + "/window_backup_camera.png");

        if (auto *backupPage = window.findChild<BackupPage *>()) {
            SettingsManager::instance()->setRenamePrefix(QStringLiteral("IMG"));
            SettingsManager::instance()->setRenameDigits(4);
            SettingsManager::instance()->setRenameCustom(QStringLiteral("婚礼"));
            backupPage->refreshFromSettings();
            wait(300);

            // 滚到「文件重命名」这一组，让三个胶囊和实时预览都在画面里
            if (auto *scroll = backupPage->findChild<QScrollArea *>()) {
                for (QGroupBox *group : backupPage->findChildren<QGroupBox *>()) {
                    if (group->title().startsWith(QStringLiteral("文件重命名"))) {
                        scroll->ensureWidgetVisible(group, 0, 60);
                        break;
                    }
                }
                wait(300);
            }
            saveWidget(&window, outDir + "/window_rename.png");
            SettingsManager::instance()->setRenameCustom(QString());
            backupPage->refreshFromSettings();

            // 双击「前缀」胶囊应该弹出编辑框：真发一次双击，抓到弹窗后关掉
            RenameChip *prefixChip = nullptr;
            for (RenameChip *chip : backupPage->findChildren<RenameChip *>()) {
                if (chip->text().startsWith(QStringLiteral("前缀"))) {
                    prefixChip = chip;
                    break;
                }
            }
            if (prefixChip) {
                const QPoint local = prefixChip->rect().center();
                QTimer::singleShot(800, [&]() {
                    for (QWidget *w : QApplication::topLevelWidgets()) {
                        if (w->inherits("QInputDialog") && w->isVisible()) {
                            saveWidget(w, outDir + "/dialog_rename.png");
                            w->close();
                        }
                    }
                });
                QMouseEvent dblClick(QEvent::MouseButtonDblClick, QPointF(local),
                                     QPointF(prefixChip->mapToGlobal(local)), Qt::LeftButton,
                                     Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(prefixChip, &dblClick);
                QTextStream(stdout) << "rename chip dblclick handled\n";
            }
        }

        flowCanvas->setFlowConfig({"year", "month", "day", "ext"});
        wait(200);
    }

    const QList<QPushButton *> buttons = window.findChildren<QPushButton *>();
    QStringList navOrder{"备份", "历史", "事件", "设置", "关于"};
    QList<QPushButton *> navButtons;
    for (const QString &name : navOrder) {
        for (QPushButton *button : buttons) {
            if (button->objectName() == "navButton" && button->text() == name)
                navButtons.append(button);
        }
    }

    QStringList pages{"backup", "history", "events", "settings", "about"};
    for (int i = 0; i < navButtons.size() && i < pages.size(); ++i) {
        if (i == 0)
            continue;
        QMetaObject::invokeMethod(navButtons.at(i), "click", Qt::DirectConnection);
        wait(420);
        saveWidget(&window, QString("%1/window_%2.png").arg(outDir, pages.at(i)));

        // 设置页的「更新」分组在最下面，单独滚过去截一张（检查更新 / 更新日志两个按钮）
        if (pages.at(i) == QStringLiteral("settings")) {
            auto *settingsPage = window.findChild<SettingsPage *>();
            auto *scroll = settingsPage
                               ? settingsPage->findChild<QScrollArea *>(QStringLiteral("pageScroll"))
                               : nullptr;
            if (scroll) {
                QWidget *updateGroup = nullptr;
                for (QGroupBox *group : scroll->widget()->findChildren<QGroupBox *>()) {
                    if (group->title() == QStringLiteral("更新")) {
                        updateGroup = group;
                        break;
                    }
                }
                if (updateGroup) {
                    scroll->ensureWidgetVisible(updateGroup, 0, 40);
                    wait(350);
                    saveWidget(&window, outDir + "/window_settings_update.png");
                    scroll->verticalScrollBar()->setValue(0);
                    wait(200);
                }
            }
        }
    }

    QTextStream(stdout) << "step: changelog\n";
    {
        ChangelogDialog dialog(&window);
        dialog.show();
        wait(600);
        saveWidget(&dialog, outDir + "/changelog.png");

        if (clickSection(&dialog, QStringLiteral("changelogHistorySection"))) {
            wait(450);
            saveWidget(&dialog, outDir + "/changelog_history.png");
        }
        if (clickSection(&dialog, QStringLiteral("changelogRelease_1.0.7"))) {
            wait(450);
            saveWidget(&dialog, outDir + "/changelog_history_open.png");
        }
        if (clickSection(&dialog, QStringLiteral("changelogTodoSection"))) {
            wait(450);
            if (auto *scroll = dialog.findChild<QScrollArea *>(QStringLiteral("changelogScroll")))
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            wait(250);
            saveWidget(&dialog, outDir + "/changelog_todo.png");
        }
        dialog.close();
        wait(200);
    }

    QTextStream(stdout) << "step: guide\n";
    {
        // 走真实入口：关于页的「使用引导」按钮
        QPushButton *guideBtn = nullptr;
        for (QPushButton *b : window.findChildren<QPushButton *>()) {
            if (b->text() == QStringLiteral("使用引导")) {
                guideBtn = b;
                break;
            }
        }
        if (guideBtn) {
            QMetaObject::invokeMethod(guideBtn, "click", Qt::DirectConnection);
            wait(700);
            saveWidget(&window, outDir + "/guide_step1.png");

            auto clickGuideButton = [&window](const QString &name) {
                for (QPushButton *b : window.findChildren<QPushButton *>()) {
                    if (b->objectName() == name) {
                        QMetaObject::invokeMethod(b, "click", Qt::DirectConnection);
                        return true;
                    }
                }
                return false;
            };

            for (int i = 0; i < 3; ++i) {
                clickGuideButton(QStringLiteral("guideNext"));
                wait(650);
            }
            saveWidget(&window, outDir + "/guide_step4.png");

            for (int i = 0; i < 3; ++i) {
                clickGuideButton(QStringLiteral("guideNext"));
                wait(650);
            }
            saveWidget(&window, outDir + "/guide_step7.png");

            for (int i = 0; i < 3; ++i) {
                clickGuideButton(QStringLiteral("guideNext"));
                wait(650);
            }
            saveWidget(&window, outDir + "/guide_last.png");

            clickGuideButton(QStringLiteral("guideSkip"));
            wait(300);
        } else {
            QTextStream(stdout) << "  FAILED: no 使用引导 button\n";
        }
    }

    QTextStream(stdout) << "step: update window\n";
    {
        // 用演示数据（不联网）截一张更新窗口
        qputenv("CJI_UPDATE_DEMO", "1");
        QPushButton *checkBtn = nullptr;
        for (QPushButton *b : window.findChildren<QPushButton *>()) {
            if (b->text() == QStringLiteral("立即检查更新")) {
                checkBtn = b;
                break;
            }
        }
        if (checkBtn) {
            QMetaObject::invokeMethod(checkBtn, "click", Qt::DirectConnection);
            wait(900);
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (w->objectName() == QStringLiteral("updateDialog")) {
                    saveWidget(w, outDir + "/update_dialog.png");
                    // 小铃铛在摇摆：隔一会儿再抓一帧，两帧应该不同
                    wait(420);
                    saveWidget(w, outDir + "/update_dialog_b.png");
                    w->close();
                    break;
                }
            }
        } else {
            QTextStream(stdout) << "  FAILED: no 立即检查更新 button\n";
        }
        qunsetenv("CJI_UPDATE_DEMO");
        wait(300);
    }

    QTextStream(stdout) << "step: splash video\n";
    {
        // 自定义视频启动动画：用现成的 MP4 试播，抓一帧看有没有真的解码出来
        SettingsManager *sm = SettingsManager::instance();
        // 用环境变量指定测试视频，别把私人文件路径写进源码（发布源码包也不会带出去）
        const QString videoPath = qEnvironmentVariable("CJI_TEST_VIDEO");
        if (!videoPath.isEmpty() && QFileInfo::exists(videoPath)) {
            sm->setSplashPreset(QStringLiteral("custom"));
            sm->setSplashMediaPath(videoPath);
            sm->setSplashDurationMs(4000);
            SplashWindow splash;
            splash.start();
            wait(1500);
            saveWidget(&splash, outDir + "/splash_video.png");
            splash.finishNow();
            wait(200);
            sm->setSplashMediaPath(QString());
            sm->setSplashPreset(QStringLiteral("swirl"));
        } else {
            QTextStream(stdout) << "  skip (no test mp4)\n";
        }
    }

    QTextStream(stdout) << "step: splash preview\n";
    {
        // 设置页「预览启动动画」按钮 -> 弹一个 SplashWindow；抓两帧看动画有没有在动
        SettingsManager *sm = SettingsManager::instance();
        sm->setSplashPreset(QStringLiteral("flash"));
        sm->setSplashDurationMs(2600);

        QPushButton *previewBtn = nullptr;
        for (QPushButton *b : window.findChildren<QPushButton *>()) {
            if (b->text() == QStringLiteral("预览启动动画")) {
                previewBtn = b;
                break;
            }
        }

        if (previewBtn) {
            QMetaObject::invokeMethod(previewBtn, "click", Qt::DirectConnection);
            SplashWindow *splash = nullptr;
            for (int attempt = 0; attempt < 20 && !splash; ++attempt) {
                wait(100);
                for (QWidget *w : QApplication::topLevelWidgets()) {
                    if (auto *s = qobject_cast<SplashWindow *>(w)) {
                        splash = s;
                        break;
                    }
                }
            }
            wait(400);
            if (splash) {
                QTextStream(stdout) << "  splash window: "
                                    << splash->geometry().x() << "," << splash->geometry().y()
                                    << " " << splash->width() << "x" << splash->height()
                                    << " visible=" << splash->isVisible() << "\n";
                saveWidget(splash, outDir + "/splash_preview_a.png");
                wait(700);
                saveWidget(splash, outDir + "/splash_preview_b.png");
                splash->finishNow();
                wait(200);
            } else {
                QTextStream(stdout) << "  FAILED: no SplashWindow appeared\n";
            }
        }
        sm->setSplashPreset(QStringLiteral("swirl"));
    }

    if (withSplash) {
        QTextStream(stdout) << "step: splash\n";
        // 13 套启动动画各截一张（截在进度 ~45% 的位置）
        SettingsManager *sm = SettingsManager::instance();
        sm->setSplashDurationMs(2200);
        for (const QString &key : SplashPresets::keys()) {
            sm->setSplashPreset(key);
            QTextStream(stdout) << "preset requested: " << key << " -> " << sm->splashPreset() << "\n";
            SplashWindow splash;
            splash.start();
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 1050) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(10);
            }
            saveWidget(&splash, QString("%1/splash_%2.png").arg(outDir, key));
            splash.finishNow();
            wait(120);
        }
        sm->setSplashPreset(QStringLiteral("swirl"));
    } else {
        QTextStream(stdout)
            << "step: splash skipped（启动动画已定稿；要重新截图就加 --splash）\n";
    }

    // 打字机提示
    {
        TypewriterTooltip *tip = TypewriterTooltip::instance();
        tip->showText("关闭窗口时：最小化到托盘＝程序缩到右下角继续运行，定时提醒才会生效；"
                      "直接退出＝关掉窗口就结束进程。",
                      &window);
        wait(1400);
        saveWidget(tip, outDir + "/tooltip.png");
        tip->dismiss();
        wait(200);
    }

    QTextStream(stdout) << "step: dialogs\n";
    // 暗色消息框（在 exec 的嵌套事件循环里抓图）
    {
        QTimer::singleShot(600, [&]() {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (w->windowTitle() == QStringLiteral("发现新版本")) {
                    saveWidget(w, outDir + "/dialog_update.png");
                    w->close();
                }
            }
        });
        DarkDialog::questionWithLink(
            &window, QStringLiteral("发现新版本"),
            QStringLiteral("发现新版本 v1.0.0，当前版本 0.3.0。\n\n更新内容：\n"
                           "这个版本的更新说明在 GitHub 上存成了乱码，可以点下面的「在浏览器里查看」看原文。\n\n"
                           "要现在下载并安装吗？"),
            "立即下载安装", "稍后再说", "在浏览器里查看",
            QUrl("https://github.com/SwiQk-ovo/Cjibackup/releases/tag/v1.0.0"));
    }

    {
        QTimer::singleShot(600, [&]() {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (w->windowTitle() == QStringLiteral("备份完成")) {
                    saveWidget(w, outDir + "/dialog_info.png");
                    w->close();
                }
            }
        });
        DarkDialog::information(&window, QStringLiteral("备份完成"),
                                QStringLiteral("备份完成：128 个文件 · 3.4 GB · 已校验"));
    }

    QTextStream(stdout) << "step: typewriter\n";
    // 打字机：确认显示完整之后不会再自己删字
    {
        TypewriterTooltip *tip = TypewriterTooltip::instance();
        tip->showText(QStringLiteral("备份完成后会逐个比对源文件和目标文件的大小，确认没有漏拷。"),
                      &window);
        wait(3000);
        QTextStream(stdout) << "typewriter visible after 3s: ["
                            << tip->visibleText() << "]\n";
        saveWidget(tip, outDir + "/tooltip_full.png");
        tip->dismiss();
        wait(400);
        QTextStream(stdout) << "typewriter after dismiss: [" << tip->visibleText() << "]\n";
    }

    QTextStream(stdout) << "step: filedialog\n";
    // 非原生文件对话框（暗色）预览
    {
        QFileDialog dialog(&window, QStringLiteral("选择 DCIM 文件夹"), QDir::homePath());
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        dialog.resize(900, 560);
        dialog.show();
        wait(600);
        saveWidget(&dialog, outDir + "/filedialog.png");
        dialog.close();
    }

    // 应用内浮层提示
    {
        QTextStream(stdout) << "step: toast\n";
        Toast::showToast(QStringLiteral("备份完成"),
                         QStringLiteral("128 个文件 · 3.4 GB · 已校验"),
                         Toast::Success, 60000,
                         QStringLiteral("打开主界面"), []() {});
        wait(1200);
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (w->inherits("Toast")) {
                saveWidget(w, outDir + "/toast.png");
                break;
            }
        }
        Toast::dismissAll();
        wait(300);
    }

    // 更新推送（后台检查发现新版本时的右下角浮层）
    {
        QTextStream(stdout) << "step: update toast\n";
        Toast::showToast(QStringLiteral("发现新版本 v1.1.0"),
                         QStringLiteral("点这里看看更新了什么，可以一键升级。"),
                         Toast::Info, 60000, QStringLiteral("查看更新"), []() {});
        wait(1200);
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (w->inherits("Toast")) {
                saveWidget(w, outDir + "/toast_update.png");
                break;
            }
        }
        Toast::dismissAll();
        wait(300);
    }

    // 新的文件选择器
    {
        QTextStream(stdout) << "step: filepicker\n";
        FilePickerDialog picker(FilePickerDialog::PickFile, &window,
                                QStringLiteral("选择启动动画"), QDir::homePath());
        picker.setNameFilter(QStringLiteral("动图 (*.gif *.webp *.apng *.png);;所有文件 (*.*)"));
        picker.show();
        wait(600);
        saveWidget(&picker, outDir + "/filepicker.png");
        picker.close();

        // 文件夹模式也截一张（这里之前会多出一个飘在左上角的类型下拉框）
        FilePickerDialog dirPicker(FilePickerDialog::PickDirectory, &window,
                                   QStringLiteral("选择目标文件夹"), QDir::homePath());
        dirPicker.show();
        wait(600);
        saveWidget(&dirPicker, outDir + "/filepicker_dir.png");
        dirPicker.close();
    }

    QTextStream(stdout) << "done\n";
    app.quit();
    return 0;
}
