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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QToolButton>
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

static void saveWidgetFixed(QWidget *widget, const QString &path, int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(1.0);
    img.fill(QColor(0x16, 0x1A, 0x23));
    QPainter p(&img);
    widget->render(&p, QPoint(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
    p.end();
    img.save(path);
    QTextStream(stdout) << "saved " << path << " (" << img.width() << "x" << img.height() << ")\n";
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
    // 截图要求 1:1 像素，先把 DPI 缩放摁掉（必须在 QApplication 之前）
    qputenv("QT_SCALE_FACTOR", QByteArray("1"));
    qputenv("QT_ENABLE_HIGHDPI_SCALING", QByteArray("0"));

    QApplication app(argc, argv);
    app.setOrganizationName("Cjibackup");
    app.setApplicationName("CjiShotTool");
    Localization::installChineseTranslations(app);

    // 用法：shot_tool.exe [输出目录] [--splash]
    //   默认不重放启动动画（那 13 套已经定稿，跑一遍要二十多秒）
    //   要看动画截图时才加 --splash
    QString outDir = QDir::currentPath();
    bool withSplash = false;
    bool promo = false;   // 宣传片用：1920x1080 真实界面 + 备份完成态
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--splash") || arg == QStringLiteral("--full"))
            withSplash = true;
        else if (arg == QStringLiteral("--promo"))
            promo = true;
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

    if (promo) {
        SettingsManager *sm = SettingsManager::instance();
        sm->setSourcePath(QStringLiteral("E:/C极备份测试环境/拷贝源"));
        sm->setTargetPath(QStringLiteral("E:/C极备份测试环境/目标"));
    }

    MainWindow window;
    window.resize(promo ? 1920 : 1000, promo ? 1080 : 680);
    window.show();
    wait(700);

    if (promo) {
        // 宣传片：先拍「备份」页，再真跑一次备份拍「备份完成」
        QTextStream(stdout) << "window=" << window.width() << "x" << window.height()
                            << " dpr=" << window.devicePixelRatioF() << "\n";
        auto *page0 = window.findChild<BackupPage *>();
        QScrollArea *scroll = nullptr;
        if (page0) {
            for (QScrollArea *sa : page0->findChildren<QScrollArea *>()) {
                if (!sa->parent() || sa->objectName().isEmpty()) continue;
                QTextStream(stdout) << "  scroll(" << sa->objectName() << ") viewport="
                                    << sa->viewport()->width() << "x" << sa->viewport()->height()
                                    << " content=" << (sa->widget() ? sa->widget()->width() : 0) << "x"
                                    << (sa->widget() ? sa->widget()->height() : 0) << "\n";
                if (!scroll && sa->widget() && sa->widget()->width() > 800) scroll = sa;
            }
            QTextStream(stdout) << "  page width=" << page0->width() << "\n";
        }
        if (scroll) scroll->verticalScrollBar()->setValue(0);
        wait(300);
        saveWidgetFixed(&window, outDir + "/app-screenshot.png", 1920, 1080);
        if (auto *page = window.findChild<BackupPage *>()) {
            QMetaObject::invokeMethod(page, "startBackup", Qt::DirectConnection);
            for (int i = 0; i < 160; ++i) {
                wait(250);
                bool done = false;
                for (QLabel *label : page->findChildren<QLabel *>()) {
                    if (label->text().contains(QStringLiteral("校验通过"))
                        || label->text().contains(QStringLiteral("校验发现"))) {
                        done = true;
                        break;
                    }
                }
                if (done) break;
            }
            for (QLabel *label : page->findChildren<QLabel *>()) {
                const QString txt = label->text();
                if (txt.contains(QStringLiteral("校验通过")) || txt.contains(QStringLiteral("校验发现")))
                    QTextStream(stdout) << "notice: " << txt << "\n";
            }
            // 滚到「传输进度」那一块，让完成状态出现在画面里
            if (scroll) {
                for (QGroupBox *box : page->findChildren<QGroupBox *>()) {
                    if (box->title().contains(QStringLiteral("传输进度"))) {
                        scroll->ensureWidgetVisible(box, 0, 30);
                        break;
                    }
                }
                wait(400);
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
            }
            wait(1200);
        }
        saveWidgetFixed(&window, outDir + "/backup-done.png", 1920, 1080);
        // 宣传片要用的软件 LOGO（透明背景）
        AppIcon::logoPixmap(1024).save(outDir + "/logo-1024.png", "PNG");
        QTextStream(stdout) << "logo saved\n";

        // 各页面 1920x1080 真截图，宣传片逐个功能展示用
        {
            const QStringList navOrder{"备份", "素材", "历史", "事件", "设置", "关于"};
            const QStringList pageFiles{"pages/backup", "pages/history", "pages/events",
                                        "pages/settings", "pages/about"};
            QDir().mkpath(outDir + "/pages");
            QList<QPushButton *> navs;
            for (const QString &name : navOrder) {
                for (QPushButton *b : window.findChildren<QPushButton *>()) {
                    if (b->objectName() == QStringLiteral("navButton") && b->text() == name) {
                        navs.append(b);
                        break;
                    }
                }
            }
            for (int i = 0; i < navs.size() && i < pageFiles.size(); ++i) {
                QMetaObject::invokeMethod(navs.at(i), "click", Qt::DirectConnection);
                wait(450);
                saveWidgetFixed(&window, outDir + "/" + pageFiles.at(i) + ".png", 1920, 1080);

                // 宣传片取景框要用精确坐标，这里把关键控件的窗口坐标打出来
                QTextStream(stdout) << "PAGE " << pageFiles.at(i) << "\n";
                for (QGroupBox *gb : window.findChildren<QGroupBox *>()) {
                    if (!gb->isVisible() || gb->title().isEmpty())
                        continue;
                    const QPoint p = gb->mapTo(&window, QPoint(0, 0));
                    QTextStream(stdout) << "RECT gb|" << gb->title() << "|"
                                        << p.x() << "," << p.y() << "," << gb->width() << "," << gb->height() << "\n";
                }
                QRect chips;
                int chipCount = 0;
                for (QPushButton *b : window.findChildren<QPushButton *>()) {
                    if (b->objectName() == QStringLiteral("flowChip") && b->isVisible()) {
                        const QRect r(b->mapTo(&window, QPoint(0, 0)), b->size());
                        chips = chipCount ? chips.united(r) : r;
                        ++chipCount;
                    }
                }
                if (chipCount > 0)
                    QTextStream(stdout) << "RECT chips|rename|" << chips.x() << "," << chips.y() << ","
                                        << chips.width() << "," << chips.height() << "\n";
                QRect actions;
                int actCount = 0;
                for (const QString &name : {QStringLiteral("开始传输"), QStringLiteral("暂停"), QStringLiteral("终止")}) {
                    for (QPushButton *b : window.findChildren<QPushButton *>()) {
                        if (b->isVisible() && b->text() == name) {
                            const QRect r(b->mapTo(&window, QPoint(0, 0)), b->size());
                            actions = actCount ? actions.united(r) : r;
                            ++actCount;
                            break;
                        }
                    }
                }
                for (QPushButton *b : window.findChildren<QPushButton *>()) {
                    if (b->isVisible() && b->text() == QStringLiteral("开始传输")) {
                        QWidget *bar = b->parentWidget();
                        if (bar) {
                            const QPoint bp = bar->mapTo(&window, QPoint(0, 0));
                            QTextStream(stdout) << "RECT actionbar|bar|" << bp.x() << "," << bp.y() << ","
                                                << bar->width() << "," << bar->height() << "\n";
                        }
                        const QPoint p2 = b->mapTo(&window, QPoint(0, 0));
                        QTextStream(stdout) << "RECT start|btn|" << p2.x() << "," << p2.y() << ","
                                            << b->width() << "," << b->height() << "\n";
                        break;
                    }
                }
                if (actCount > 0)
                    QTextStream(stdout) << "RECT actions|buttons|" << actions.x() << "," << actions.y() << ","
                                        << actions.width() << "," << actions.height() << "\n";
                for (QListWidget *lw : window.findChildren<QListWidget *>()) {
                    if (!lw->isVisible())
                        continue;
                    const QPoint p = lw->mapTo(&window, QPoint(0, 0));
                    QTextStream(stdout) << "RECT list|" << pageFiles.at(i) << "|" << p.x() << "," << p.y()
                                        << "," << lw->width() << "," << lw->height() << "\n";
                }
                for (QWidget *fc : window.findChildren<QWidget *>()) {
                    if (fc->isVisible() && QString::fromLatin1(fc->metaObject()->className()) == QStringLiteral("FlowCanvas")) {
                        const QPoint p = fc->mapTo(&window, QPoint(0, 0));
                        QTextStream(stdout) << "RECT canvas|flow|" << p.x() << "," << p.y() << ","
                                            << fc->width() << "," << fc->height() << "\n";
                    }
                }
            }
            // 素材页单独抓一张（缩略图是异步加载的，多等一会儿）
            for (QPushButton *b : window.findChildren<QPushButton *>()) {
                if (b->objectName() == QStringLiteral("navButton") && b->text() == QStringLiteral("素材")) {
                    QMetaObject::invokeMethod(b, "click", Qt::DirectConnection);
                    wait(1200);

                    wait(8000);
                    // 先空跑一次渲染把「错峰淡入」跑完，再存真正的一张
                    saveWidgetFixed(&window, outDir + "/.album_warmup.png", 1920, 1080);
                    wait(900);
                    saveWidgetFixed(&window, outDir + "/pages/album.png", 1920, 1080);
                    QFile::remove(outDir + "/.album_warmup.png");

                    // 再分别抓列表 / 平铺两种视图
                    QList<QToolButton *> viewBtns;
                    for (QToolButton *tb : window.findChildren<QToolButton *>()) {
                        if (tb->objectName() == QStringLiteral("albumViewBtn"))
                            viewBtns.append(tb);
                    }
                    if (viewBtns.size() >= 3) {
                        viewBtns.at(1)->click();
                        wait(900);
                        saveWidgetFixed(&window, outDir + "/pages/album_list.png", 1920, 1080);
                        viewBtns.at(2)->click();
                        wait(900);
                        saveWidgetFixed(&window, outDir + "/pages/album_tile.png", 1920, 1080);
                        viewBtns.at(0)->click();
                        wait(400);
                    }
                    break;
                }
            }
            QMetaObject::invokeMethod(navs.at(0), "click", Qt::DirectConnection);
            wait(300);
        }
        return 0;
    }

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

    // 2.0.3：真模拟「把月拖到年身上」→ 弹分隔符框 → 选 . 并确定，检查合并结果
    if (auto *flowCanvas = window.findChild<FlowCanvas *>()) {
        flowCanvas->setFlowConfig({"year", "month", "day", "ext"});
        wait(350);

        auto *scene = flowCanvas->findChild<FlowScene *>();
        auto *view = flowCanvas->findChild<QGraphicsView *>();
        if (scene && view) {
            FlowNodeItem *year = nullptr;
            FlowNodeItem *month = nullptr;
            for (QGraphicsItem *item : scene->items()) {
                auto *node = dynamic_cast<FlowNodeItem *>(item->toGraphicsObject());
                if (!node)
                    continue;
                if (node->title() == QStringLiteral("年"))
                    year = node;
                else if (node->title() == QStringLiteral("月"))
                    month = node;
            }

            if (year && month) {
                const QPoint start =
                    view->mapFromScene(month->mapToScene(month->boundingRect().center()));
                const QPoint end =
                    view->mapFromScene(year->mapToScene(year->boundingRect().center()));

                QTimer::singleShot(700, [&]() {
                    for (QWidget *w : QApplication::topLevelWidgets()) {
                        if (w->objectName() != QStringLiteral("separatorDialog") || !w->isVisible())
                            continue;
                        saveWidget(w, outDir + "/dialog_merge_choose.png");
                        for (QPushButton *b : w->findChildren<QPushButton *>()) {
                            if (b->property("sep").toString() == QStringLiteral(".")) {
                                b->click();
                                break;
                            }
                        }
                        for (QPushButton *b : w->findChildren<QPushButton *>()) {
                            if (b->objectName() == QStringLiteral("dialogPrimary")) {
                                b->click();
                                break;
                            }
                        }
                    }
                });

                QMouseEvent press(QEvent::MouseButtonPress, start,
                                  view->viewport()->mapToGlobal(start), Qt::LeftButton,
                                  Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(view->viewport(), &press);
                for (int step = 1; step <= 12; ++step) {
                    const QPoint p = start + (end - start) * step / 12;
                    QMouseEvent move(QEvent::MouseMove, p, view->viewport()->mapToGlobal(p),
                                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(view->viewport(), &move);
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
                    QThread::msleep(12);
                }
                QMouseEvent release(QEvent::MouseButtonRelease, end,
                                    view->viewport()->mapToGlobal(end), Qt::LeftButton,
                                    Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(view->viewport(), &release);
                wait(1200);
                saveWidget(&window, outDir + "/window_backup_merged_drag.png");

                // 双击合并节点 → 把分隔符改成 _
                FlowNodeItem *merged = nullptr;
                for (QGraphicsItem *item : scene->items()) {
                    auto *node = dynamic_cast<FlowNodeItem *>(item->toGraphicsObject());
                    if (node && node->isMerged()) {
                        merged = node;
                        break;
                    }
                }
                if (merged) {
                    const QPoint center =
                        view->mapFromScene(merged->mapToScene(merged->boundingRect().center()));
                    QTimer::singleShot(600, [&]() {
                        for (QWidget *w : QApplication::topLevelWidgets()) {
                            if (w->objectName() != QStringLiteral("separatorDialog")
                                || !w->isVisible())
                                continue;
                            for (QPushButton *b : w->findChildren<QPushButton *>()) {
                                if (b->property("sep").toString() == QStringLiteral("_")) {
                                    b->click();
                                    break;
                                }
                            }
                            for (QPushButton *b : w->findChildren<QPushButton *>()) {
                                if (b->objectName() == QStringLiteral("dialogPrimary")) {
                                    b->click();
                                    break;
                                }
                            }
                        }
                    });
                    QMouseEvent dblClick(QEvent::MouseButtonDblClick, center,
                                         view->viewport()->mapToGlobal(center), Qt::LeftButton,
                                         Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(view->viewport(), &dblClick);
                    wait(900);
                    saveWidget(&window, outDir + "/window_backup_merged_sep.png");

                    // 再拆开，检查能回到四个独立节点
                    scene->unmergeNode(merged);
                    wait(600);
                    saveWidget(&window, outDir + "/window_backup_unmerged.png");
                    QTextStream(stdout) << "after unmerge: " << flowCanvas->flowConfig().join(" | ")
                                        << "\n";
                }
            } else {
                QTextStream(stdout) << "  merge drag skipped（找不到年 / 月节点）\n";
            }
        }

        flowCanvas->setFlowConfig({"year", "month", "day", "ext"});
        wait(200);
    }

    // 2.0.3：合并节点（年/月/日 用 . 连成一层）+ 分隔符选择弹窗
    if (auto *flowCanvas = window.findChild<FlowCanvas *>()) {
        flowCanvas->setFlowConfig({FlowPath::encodeGroup(QStringLiteral("."),
                                                        {"year", "month", "day"}),
                                   "camera", "ext"});
        wait(420);
        saveWidget(&window, outDir + "/window_backup_merged.png");

        QString picked;
        QTimer::singleShot(800, [&]() {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (w->objectName() == QStringLiteral("separatorDialog") && w->isVisible()) {
                    saveWidget(w, outDir + "/dialog_separator.png");
                    w->close();
                }
            }
        });
        flowCanvas->askMergeSeparator(QStringLiteral("-"), {"2026", "10", "31"}, &picked);
        wait(300);

        // 连续两个合并节点：验证链路没断（config 里应该能读回两条 merge）
        flowCanvas->setFlowConfig({FlowPath::encodeGroup(QStringLiteral("."), {"year", "month"}),
                                   FlowPath::encodeGroup(QStringLiteral("_"), {"day", "ext"})});
        wait(350);
        saveWidget(&window, outDir + "/window_backup_merged_two.png");
        const QStringList twoMerge = flowCanvas->flowConfig();
        QTextStream(stdout) << "two-merge config: " << twoMerge.join(" | ") << "\n";
        if (twoMerge.size() != 2 || !FlowPath::isMergeEntry(twoMerge.at(0))
            || !FlowPath::isMergeEntry(twoMerge.at(1))) {
            QTextStream(stdout) << "  FAILED: two-merge chain broken\n";
        }

        flowCanvas->setFlowConfig({"year", "month", "day", "ext"});
        wait(200);
    }

    // 2.1.0：把「相机型号」从分类面板直接拖到「月」节点上 → 合并进去
    if (auto *flowCanvas = window.findChild<FlowCanvas *>()) {
        flowCanvas->setFlowConfig({"year", "month", "ext"});
        wait(350);

        auto *scene = flowCanvas->findChild<FlowScene *>();
        auto *view = flowCanvas->findChild<QGraphicsView *>();
        FlowNodeItem *target = nullptr;
        if (scene) {
            for (QGraphicsItem *item : scene->items()) {
                auto *node = dynamic_cast<FlowNodeItem *>(item->toGraphicsObject());
                if (node && node->title() == QStringLiteral("月")) {
                    target = node;
                    break;
                }
            }
        }

        if (target && view) {
            const QPoint pos =
                view->mapFromScene(target->mapToScene(target->boundingRect().center()));
            QMimeData *mime = new QMimeData;
            mime->setData(FlowMime::Format, QByteArray("camera"));
            mime->setText(QStringLiteral("相机型号"));

            // 弹分隔符框时自动点「确定」
            QTimer::singleShot(800, [&]() {
                for (QWidget *w : QApplication::topLevelWidgets()) {
                    if (w->objectName() != QStringLiteral("separatorDialog") || !w->isVisible())
                        continue;
                    saveWidget(w, outDir + "/dialog_palette_drop.png");
                    for (QPushButton *b : w->findChildren<QPushButton *>()) {
                        if (b->objectName() == QStringLiteral("dialogPrimary")) {
                            b->click();
                            break;
                        }
                    }
                }
            });

            QDragEnterEvent enter(pos, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &enter);
            QDropEvent drop(QPointF(pos), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &drop);
            wait(1100);
            saveWidget(&window, outDir + "/window_palette_drop_merged.png");
            QTextStream(stdout) << "palette drop config: " << flowCanvas->flowConfig().join(" | ")
                                << "\n";
            delete mime;
        } else {
            QTextStream(stdout) << "  palette drop skipped（找不到月节点）\n";
        }

        flowCanvas->setFlowConfig({"year", "month", "day", "ext"});
        wait(200);
    }

    // 2.1.0：危险操作两次确认（第二次确认按钮换位置）
    QTextStream(stdout) << "step: danger confirm\n";
    {
        QTimer::singleShot(500, [&]() {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (w->windowTitle() != QStringLiteral("删除节点") || !w->isVisible())
                    continue;
                saveWidget(w, outDir + "/dialog_danger1.png");
                for (QPushButton *b : w->findChildren<QPushButton *>()) {
                    if (b->objectName() == QStringLiteral("dialogPrimary")) {
                        b->click();
                        break;
                    }
                }
            }
        });
        QTimer::singleShot(1300, [&]() {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (w->windowTitle() != QStringLiteral("再确认一次") || !w->isVisible())
                    continue;
                saveWidget(w, outDir + "/dialog_danger2.png");
                for (QPushButton *b : w->findChildren<QPushButton *>()) {
                    if (b->objectName() == QStringLiteral("dialogPrimary")) {
                        b->click();
                        break;
                    }
                }
            }
        });
        const bool confirmed = DarkDialog::confirmTwice(
            &window, QStringLiteral("删除节点"),
            QStringLiteral("确定要删除节点「年」吗？"), QStringLiteral("删除"));
        QTextStream(stdout) << "danger confirm result: " << (confirmed ? "accepted" : "rejected")
                            << "\n";
        wait(300);
    }

    // 2.1.0：清空画布（两次确认）+ 从提示里撤销
    QTextStream(stdout) << "step: clear and undo\n";
    if (auto *flowCanvas = window.findChild<FlowCanvas *>()) {
        flowCanvas->setFlowConfig({"year", "month", "day", "ext"});
        wait(300);

        auto clickPrimary = [](const QString &title) {
            for (QWidget *w : QApplication::topLevelWidgets()) {
                if (w->windowTitle() != title || !w->isVisible())
                    continue;
                for (QPushButton *b : w->findChildren<QPushButton *>()) {
                    if (b->objectName() == QStringLiteral("dialogPrimary")) {
                        b->click();
                        return;
                    }
                }
            }
        };
        QTimer::singleShot(400, [clickPrimary]() { clickPrimary(QStringLiteral("清空画布")); });
        QTimer::singleShot(950, [clickPrimary]() { clickPrimary(QStringLiteral("再确认一次")); });

        flowCanvas->clearAll();
        wait(500);
        QTextStream(stdout) << "after clear, config size = " << flowCanvas->flowConfig().size()
                            << "\n";

        QWidget *toast = nullptr;
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (w->inherits("Toast")) {
                toast = w;
                break;
            }
        }
        if (toast) {
            saveWidget(toast, outDir + "/toast_undo.png");
            for (QPushButton *b : toast->findChildren<QPushButton *>()) {
                if (b->text() == QStringLiteral("撤销")) {
                    b->click();
                    break;
                }
            }
        } else {
            QTextStream(stdout) << "  FAILED: no undo toast\n";
        }

        wait(800);
        QTextStream(stdout) << "after undo: " << flowCanvas->flowConfig().join(" | ") << "\n";
        if (flowCanvas->flowConfig().size() != 4)
            QTextStream(stdout) << "  FAILED: undo did not restore config\n";
    }

    const QList<QPushButton *> buttons = window.findChildren<QPushButton *>();
    QStringList navOrder{"备份", "素材", "历史", "事件", "设置", "关于"};
    QList<QPushButton *> navButtons;
    for (const QString &name : navOrder) {
        for (QPushButton *button : buttons) {
            if (button->objectName() == "navButton" && button->text() == name)
                navButtons.append(button);
        }
    }

    QStringList pages{"backup", "album", "history", "events", "settings", "about"};
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

                // 2.0.3 新分类：滚到「提醒」组补一张，检查备份提示和定时提醒在一起
                for (QGroupBox *group : scroll->widget()->findChildren<QGroupBox *>()) {
                    if (group->title() == QStringLiteral("提醒")) {
                        scroll->ensureWidgetVisible(group, 0, 40);
                        wait(350);
                        saveWidget(&window, outDir + "/window_settings_reminder.png");
                        scroll->verticalScrollBar()->setValue(0);
                        wait(200);
                        break;
                    }
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

                    // 点「立即升级」→ 演示模式假造下载进度，抓一张进度条
                    for (QPushButton *b : w->findChildren<QPushButton *>()) {
                        if (b->text() == QStringLiteral("立即升级") && b->isVisible()) {
                            QMetaObject::invokeMethod(b, "click", Qt::DirectConnection);
                            break;
                        }
                    }
                    wait(700);
                    saveWidget(w, outDir + "/update_download_progress.png");
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

    QTextStream(stdout) << "step: update window single channel\n";
    {
        // 只有正式版有更新：预发布卡应该整张隐藏，正式版卡铺满
        qputenv("CJI_UPDATE_DEMO", "1");
        qputenv("CJI_UPDATE_DEMO_SINGLE", "1");
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
                    saveWidget(w, outDir + "/update_dialog_single.png");
                    w->close();
                    break;
                }
            }
        }
        qunsetenv("CJI_UPDATE_DEMO_SINGLE");
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
