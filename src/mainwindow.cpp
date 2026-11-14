#include "mainwindow.h"

#include "core/appicon.h"
#include "core/remindermanager.h"
#include "core/settingsmanager.h"
#include "core/updatemanager.h"
#include "core/winutils.h"
#include "core/version.h"
#include "pages/about_page.h"
#include "pages/album_page.h"
#include "pages/backup_page.h"
#include "pages/events_page.h"
#include "pages/history_page.h"
#include "pages/settings_page.h"
#include "widgets/darkdialog.h"
#include "widgets/squircle.h"
#include "widgets/guideoverlay.h"
#include "widgets/toast.h"
#include "widgets/updatedialog.h"
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "widgets/splashwindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QSettings>
#include <QFile>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("KOPAL");
    setWindowIcon(AppIcon::applicationIcon());
    resize(1000, 680);
    setMinimumSize(800, 550);

    setupUI();
    setupSidebar();
    setupPages();
    setupTray();
    setupReminder();
    setupUpdater();

    // 恢复上次的窗口位置；越界/超出屏幕的几何直接丢弃，改为最大化
    {
        QSettings settings;
        const QByteArray saved = settings.value(QStringLiteral("ui/geometry")).toByteArray();
        if (!saved.isEmpty() && restoreGeometry(saved)) {
            const QRect avail = screen() ? screen()->availableGeometry()
                                         : QRect(0, 0, 1280, 800);
            const QRect frame = frameGeometry();
            const bool offScreen = !avail.intersects(frame);
            const bool tooBig = frame.width() > avail.width() || frame.height() > avail.height();
            if (offScreen || tooBig) {
                resize(qMin(avail.width(), 1280), qMin(avail.height(), 800));
                move(avail.topLeft());
            } else {
                m_hasSavedGeometry = true;
                if (settings.value(QStringLiteral("ui/maximized"), false).toBool())
                    setWindowState(windowState() | Qt::WindowMaximized);
            }
        }
    }


    // 初始化滑块位置
    QTimer::singleShot(50, this, [this]() { updateSliderPosition(0); });

    WinUtils::applyDarkTitleBar(this);
}

// 背景光斑：让上面的半透明卡片看起来像玻璃（蓝，唯一彩色）
// 背景：蓝板岩渐变 + 三团大柔光（不是纯黑，避免"黑里发白"）
void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0E, 0x0E, 0x0F));
}
void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("centralWidget");
    setCentralWidget(m_centralWidget);

    // 兜底：窗口底色和内容底色一致，DPI/最大化时不会露出黑边
    {
        QPalette pal = palette();
        pal.setColor(QPalette::Window, QColor(0x0E, 0x0F, 0x11));
        setPalette(pal);
        setAutoFillBackground(true);
        m_centralWidget->setAutoFillBackground(true);
        m_centralWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        // 只给这个控件本身设背景；不带选择器会把背景传给所有子控件（输入框会变黑块）
        m_centralWidget->setStyleSheet(QStringLiteral("QWidget#centralWidget { background: #111112; }"));
    }

    m_mainLayout = new QHBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 侧边栏
    m_sidebar = new QFrame(m_centralWidget);
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(180);

    m_sidebarLayout = new QVBoxLayout(m_sidebar);
    m_sidebarLayout->setContentsMargins(0, 30, 0, 20);
    m_sidebarLayout->setSpacing(5);

    // 内容区
    m_pages = new QStackedWidget(m_centralWidget);
    m_pages->setObjectName("contentArea");

    m_mainLayout->addWidget(m_sidebar);
    m_mainLayout->addWidget(m_pages, 1);
}

void MainWindow::setupSidebar()
{
    // Logo
    m_logoLabel = new QLabel("KOPAL", m_sidebar);
    m_logoLabel->setObjectName("logoLabel");
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setFixedHeight(50);
    m_sidebarLayout->addWidget(m_logoLabel);
    m_sidebarLayout->addSpacing(20);

    // 滑块（蓝色小方块）
    m_slider = new QFrame(m_sidebar);
    m_slider->setObjectName("navSlider");
    m_slider->setFixedSize(6, 28);
    m_slider->hide();

    // 导航按钮
    const QStringList navItems = {"备份", "素材", "历史", "事件", "设置", "关于"};
    for (int i = 0; i < navItems.size(); ++i) {
        auto *btn = new QPushButton(navItems.at(i), m_sidebar);
        btn->setObjectName("navButton");
        btn->setCheckable(true);
        btn->setFixedHeight(40);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("index", i);
        connect(btn, &QPushButton::clicked, this, [this, i]() { switchPage(i); });
        m_sidebarLayout->addWidget(btn);
        m_navButtons[i] = btn;
    }

    m_sidebarLayout->addStretch();

    // 滑块动画
    m_sliderAnim = new QPropertyAnimation(m_slider, "geometry", this);
    m_sliderAnim->setDuration(200);
    m_sliderAnim->setEasingCurve(QEasingCurve::OutCubic);

    // 整页淡入用的不透明度效果（只挂一层，避免嵌套 effect 导致内容画不出来）
    m_pageFade = new QGraphicsOpacityEffect(m_pages);
    m_pageFade->setOpacity(1.0);
    m_pages->setGraphicsEffect(m_pageFade);

    m_pageFadeAnim = new QPropertyAnimation(m_pageFade, "opacity", this);
    m_pageFadeAnim->setDuration(150);
    m_pageFadeAnim->setStartValue(0.55);
    m_pageFadeAnim->setEndValue(1.0);
    m_pageFadeAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_pageFadeAnim, &QPropertyAnimation::finished, this, [this]() {
        m_pageFade->setOpacity(1.0);   // 兜底：结束一定回到完全不透明
    });

    m_navButtons[0]->setChecked(true);
}

void MainWindow::setupPages()
{
    m_backupPage = new BackupPage(this);
    m_pages->addWidget(m_backupPage);
    m_albumPage = new AlbumPage(this);
    m_pages->addWidget(m_albumPage);
    m_historyPage = new HistoryPage(this);
    m_pages->addWidget(m_historyPage);
    m_eventsPage = new EventsPage(this);
    m_pages->addWidget(m_eventsPage);
    m_settingsPage = new SettingsPage(this);
    m_pages->addWidget(m_settingsPage);
    m_aboutPage = new AboutPage(this);
    m_pages->addWidget(m_aboutPage);

    connect(m_backupPage, &BackupPage::backupCompleted, this, &MainWindow::onBackupCompleted);
    connect(m_settingsPage, &SettingsPage::checkUpdateRequested, this,
            [this]() { checkForUpdates(true); });
    connect(m_settingsPage, &SettingsPage::previewSplashRequested, this, [this]() {
        // 连点几下「预览」不要叠一地窗口：先把上一个收掉
        if (m_splashPreview) {
            m_splashPreview->finishNow();
            m_splashPreview->deleteLater();
            m_splashPreview = nullptr;
        }
        auto *splash = new SplashWindow(this);
        m_splashPreview = splash;
        connect(splash, &SplashWindow::finished, this, [this, splash]() {
            if (m_splashPreview == splash)
                m_splashPreview = nullptr;
            splash->deleteLater();
        });
        splash->start();
    });
    connect(m_aboutPage, &AboutPage::guideRequested, this, [this]() { startGuide(); });

    // 首次启动的使用引导
    m_guide = new GuideOverlay(this);
    connect(m_guide, &GuideOverlay::finished, this, [this]() {
        SettingsManager::instance()->setGuideShown(true);
    });
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(AppIcon::applicationIcon(), this);
    m_tray->setToolTip("KOPAL");

    m_trayMenu = new QMenu(this);
    QAction *openAction = m_trayMenu->addAction("打开主界面");
    connect(openAction, &QAction::triggered, this, &MainWindow::showFromTray);
    m_startAction = m_trayMenu->addAction("开始备份");
    connect(m_startAction, &QAction::triggered, this, [this]() {
        showFromTray();
        switchPage(0);
        m_backupPage->startBackup();
    });
    m_trayMenu->addSeparator();
    QAction *updateAction = m_trayMenu->addAction("检查更新");
    connect(updateAction, &QAction::triggered, this, [this]() { checkForUpdates(true); });
    m_trayMenu->addSeparator();
    QAction *quitAction = m_trayMenu->addAction("退出");
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitApplication);

    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick
                    || reason == QSystemTrayIcon::Trigger)
                    showFromTray();
            });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &MainWindow::showFromTray);
    m_tray->show();
}

void MainWindow::setupReminder()
{
    m_reminder = new ReminderManager(this);
    connect(m_reminder, &ReminderManager::reminderTriggered, this,
            &MainWindow::onReminderTriggered);
    m_reminder->start();
}

void MainWindow::setupUpdater()
{
    m_updater = new UpdateManager(this);

    connect(m_updater, &UpdateManager::statusChanged, this, [this](const QString &text) {
        if (m_settingsPage)
            m_settingsPage->setUpdateStatus(text);
    });
    connect(m_updater, &UpdateManager::updateFound, this, &MainWindow::onUpdateFound);
    connect(m_updater, &UpdateManager::upToDate, this, [this](const QString &) {
        if (m_manualUpdateCheck && m_settingsPage)
            m_settingsPage->setUpdateStatus(QStringLiteral("已经是最新版本"));
        m_manualUpdateCheck = false;
    });
    connect(m_updater, &UpdateManager::failed, this, [this](const QString &error) {
        if (m_settingsPage)
            m_settingsPage->setUpdateStatus(error);
        if (m_manualUpdateCheck) {
            DarkDialog::information(this, "检查更新", error);
            m_manualUpdateCheck = false;
        }
    });
    connect(m_updater, &UpdateManager::installerLaunched, this, [this]() {
        Toast::showToast(QStringLiteral("正在安装更新"),
                         QStringLiteral("安装包已启动，安装完成后新版本会自动生效。"),
                         Toast::Info, 7000);
    });

    // 已经打开的客户端也要能"收到推送"：每 20 分钟静默看一眼新版本
    m_updatePoll = new QTimer(this);
    m_updatePoll->setInterval(20 * 60 * 1000);
    connect(m_updatePoll, &QTimer::timeout, this, [this]() {
        if (!SettingsManager::instance()->autoCheckUpdates())
            return;
        if (m_manualUpdateCheck)
            return;                                  // 正在手动检查，别插一脚
        if (m_updateDialog && m_updateDialog->isVisible())
            return;                                  // 更新窗口开着，它会自己刷新
        checkForUpdates(false);
    });
    m_updatePoll->start();

    m_notifiedVersion = SettingsManager::instance()->notifiedVersion();

    // 启动后静默检查一次
    if (SettingsManager::instance()->autoCheckUpdates()) {
        QTimer::singleShot(2500, this, [this]() { checkForUpdates(false); });
    }
}

void MainWindow::checkForUpdates(bool manual)
{
    m_manualUpdateCheck = manual;

    if (manual) {
        if (m_settingsPage)
            m_settingsPage->setUpdateStatus(QStringLiteral("正在检查更新…"));
        // 手动检查：直接打开更新窗口，窗口自己拉全部版本
        openUpdateWindow(true);
        return;
    }

    if (m_updater)
        m_updater->checkForUpdates();
}

void MainWindow::openUpdateWindow(bool startCheck)
{
    if (!m_updateDialog) {
        m_updateDialog = new UpdateDialog(m_updater, this);
        m_updateDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_updateDialog, &QObject::destroyed, this, [this]() {
            m_updateDialog = nullptr;
        });
        connect(m_updateDialog, &UpdateDialog::checked, this, [this](const QString &summary) {
            if (m_settingsPage)
                m_settingsPage->setUpdateStatus(summary);
        });
    }

    if (m_updateDialog->isMinimized())
        m_updateDialog->showNormal();
    m_updateDialog->show();

    // 居中到主窗口
    m_updateDialog->move(frameGeometry().center() - m_updateDialog->rect().center());
    m_updateDialog->raise();
    m_updateDialog->activateWindow();

    if (startCheck)
        m_updateDialog->startCheck();
}

void MainWindow::onUpdateFound(const QString &version, const QString &notes,
                               const QString &downloadUrl, const QString &pageUrl)
{
    Q_UNUSED(notes)
    Q_UNUSED(downloadUrl)
    Q_UNUSED(pageUrl)

    if (m_settingsPage)
        m_settingsPage->setUpdateStatus(QString("发现新版本 v%1").arg(version));

    const bool manual = m_manualUpdateCheck;
    m_manualUpdateCheck = false;

    if (manual) {
        openUpdateWindow(true);
        return;
    }

    // ---- 后台推送：不抢焦点，只在右下角浮层 + 托盘气泡里提示 ----
    SettingsManager *sm = SettingsManager::instance();
    if (sm->skippedVersion() == version)
        return;                                   // 这个版本用户已经"跳过"了，别再烦

    if (m_updateDialog && m_updateDialog->isVisible()) {
        m_updateDialog->startCheck();             // 窗口正好开着：直接刷新成新数据
        return;
    }

    if (m_notifiedVersion == version)
        return;                                   // 同一个版本只提醒一次
    m_notifiedVersion = version;
    sm->setNotifiedVersion(version);

    Toast::showToast(QStringLiteral("发现新版本 v%1").arg(version),
                     QStringLiteral("点这里看看更新了什么，可以一键升级。"),
                     Toast::Info, 9000, QStringLiteral("查看更新"),
                     [this]() { openUpdateWindow(true); });

    if (m_tray)
        m_tray->showMessage(QStringLiteral("KOPAL"),
                            QStringLiteral("新版本 v%1 已发布，点一下就能升级。").arg(version),
                            QSystemTrayIcon::Information, 7000);
}

void MainWindow::onBackupCompleted(int files, qint64 bytes, bool verified)
{
    SettingsManager *sm = SettingsManager::instance();

    const QString sizeText = QString("%1 MB").arg(double(bytes) / 1024.0 / 1024.0, 0, 'f', 1);
    const QString text = QString("备份完成：%1 个文件 · %2%3")
                             .arg(files)
                             .arg(sizeText)
                             .arg(verified ? QStringLiteral(" · 已校验") : QString());

    if (sm->notifyOnComplete()) {
        Toast::showToast(QStringLiteral("备份完成"), text, Toast::Success, 8000,
                         QStringLiteral("打开主界面"), [this]() { showFromTray(); });
    }

    if (isVisible() && isActiveWindow() && !sm->notifyOnComplete()) {
        DarkDialog::information(this, "备份完成", text);
    }
}

void MainWindow::onReminderTriggered(const QDateTime &due)
{
    const QString text = QString("该备份今天的照片了！\n上次提醒时间：%1")
                             .arg(due.toString("yyyy-MM-dd HH:mm"));
    Toast::showToast(QStringLiteral("备份提醒"), text, Toast::Reminder, 12000,
                     QStringLiteral("打开主界面"), [this]() { showFromTray(); });
}

void MainWindow::prepareShow()
{
    SettingsManager *sm = SettingsManager::instance();
    if (sm->startMinimized() && m_tray) {
        hide();
        Toast::showToast(QStringLiteral("KOPAL"),
                         QStringLiteral("已在后台运行，双击托盘图标打开主界面。"),
                         Toast::Info, 5000, QStringLiteral("打开主界面"),
                         [this]() { showFromTray(); });
        return;
    }
    showFromTray();

    // 第一次打开：过一会儿弹使用引导（跳过 / 看完都会记下来，不再打扰）
#ifdef CJI_LITE
    if (false) {
#else
    if (!sm->guideShown()) {
#endif
        QTimer::singleShot(700, this, [this]() {
            if (isVisible())
                startGuide();
        });
    }
}

void MainWindow::startGuide()
{
    if (!m_guide || m_guide->isActive())
        return;

    auto pageTitle = [](QWidget *page) -> QWidget * {
        if (!page)
            return nullptr;
        return page->findChild<QLabel *>(QStringLiteral("pageTitle"));
    };

    QVector<GuideOverlay::Step> steps;

    steps.append({QStringLiteral("欢迎使用 KOPAL"),
                  QStringLiteral("插卡、选两个文件夹、点开始，照片就会按你定的规则自动归档到硬盘，\n"
                                 "拷完还会逐个校验。下面用几步把每个功能点一遍，一分钟看完。"),
                  nullptr,
                  [this]() {
                      switchPage(0);
                      if (auto *scroll = m_backupPage->findChild<QScrollArea *>(
                              QStringLiteral("pageScroll")))
                          scroll->verticalScrollBar()->setValue(0);
                  }});

    steps.append({QStringLiteral("① 源文件夹：相机存储卡"),
                  QStringLiteral("点「浏览」选卡里的 DCIM 文件夹。选完会自动读照片的 EXIF，\n"
                                 "告诉你卡里有几台相机、各拍了多少张。"),
                  [this]() { return m_backupPage->sourceGroupWidget(); },
                  [this]() { switchPage(0); }});

    steps.append({QStringLiteral("② 目标文件夹：存到硬盘哪里"),
                  QStringLiteral("选本地磁盘上的一个文件夹，照片最终就落在这里。\n"
                                 "建议挑剩余空间够的盘。"),
                  [this]() { return m_backupPage->targetGroupWidget(); },
                  [this]() { switchPage(0); }});

    steps.append({QStringLiteral("③ 画归档流程图"),
                  QStringLiteral("把上面的分类项拖进画布，从左到右就是文件夹层级。\n"
                                 "例如 「年 → 月 → 日 → 相机型号 → 后缀」会生成 2026/10/31/Nikon Z 6II/JPG。\n"
                                 "右键从一个节点拖到右边的节点可以改连线，画布下面实时预览目录。"),
                  [this]() { return m_backupPage->flowGroupWidget(); },
                  [this]() { switchPage(0); }});

    steps.append({QStringLiteral("④ 文件名规则"),
                  QStringLiteral("三个胶囊拼出文件名：前缀 + 补零序号 + 自定义内容，例如 IMG0001婚礼.jpg。\n"
                                 "左键双击任意一个胶囊就能改，右边是实时预览。"),
                  [this]() { return m_backupPage->renameGroupWidget(); },
                  [this]() { switchPage(0); }});

    steps.append({QStringLiteral("⑤ 开始传输"),
                  QStringLiteral("点「开始传输」开始拷贝，中途可以暂停 / 终止，进度区会显示速度、用时和当前文件。\n"
                                 "结束后会自动校验；只有全部完整、你也勾了格式化，才会问要不要打开格式化界面 ——\n"
                                 "是否格式化永远由你点确认，软件不会自己动手。"),
                  [this]() { return m_backupPage->actionBarWidget(); },
                  [this]() { switchPage(0); }});

    steps.append({QStringLiteral("⑥ 历史记录"),
                  QStringLiteral("每次备份的时间、文件数、大小、平均速度都记在这里，\n"
                                 "右上角「导出 CSV」可以直接用 Excel 打开。"),
                  [this, pageTitle]() {
                      if (auto *card = m_historyPage->findChild<QWidget *>(QStringLiteral("historyCard")))
                          return card;
                      return pageTitle(m_historyPage);
                  },
                  [this]() { switchPage(2); }});

    steps.append({QStringLiteral("⑦ 常用事件"),
                  QStringLiteral("把常拍的类型（婚礼、校运会……）存成事件。\n"
                                 "画流程图选「事件」节点时直接从这里挑，不用每次手打。"),
                  [this, pageTitle]() {
                      if (auto *card = m_eventsPage->findChild<QWidget *>(QStringLiteral("eventsCard")))
                          return card;
                      return pageTitle(m_eventsPage);
                  },
                  [this]() { switchPage(3); }});

    steps.append({QStringLiteral("⑧ 设置"),
                  QStringLiteral("退出行为、线程数、校验开关、启动动画、定时提醒、自动更新，都在这一页。\n"
                                 "改完立即生效，不用重启。"),
                  [this, pageTitle]() { return pageTitle(m_settingsPage); },
                  [this]() { switchPage(4); }});

    steps.append({QStringLiteral("⑨ 关于页"),
                  QStringLiteral("版本号、作者、B 站 / GitHub 链接、数据目录都在这一页。\n"
                                 "想再看一遍这段引导，点「使用引导」就行。"),
                  [this]() { return m_aboutPage->guideButtonWidget(); },
                  [this]() { switchPage(5); }});

    switchPage(0);
    m_guide->start(steps);
    m_guide->raise();
}

void MainWindow::showFromTray()
{
    showNormal();
    if (!m_hasSavedGeometry && !isMaximized())
        showMaximized();
    raise();
    activateWindow();
}

void MainWindow::switchPage(int index)
{
    if (index < 0 || index >= 6 || index == m_currentIndex)
        return;

    m_navButtons[m_currentIndex]->setChecked(false);
    m_navButtons[index]->setChecked(true);
    m_pages->setCurrentIndex(index);
    updateSliderPosition(index);
    m_currentIndex = index;

    // 页面切换：只对 stack 整体做一次轻微淡入。
    // 起点 0.55 而不是 0 —— 快速连点时动画被打断也不会把页面留在全透明状态。
    m_pageFadeAnim->stop();
    m_pageFade->setOpacity(0.55);
    m_pageFadeAnim->start();
}

void MainWindow::updateSliderPosition(int index)
{
    if (index < 0 || index >= 6 || !m_navButtons[index])
        return;

    QPushButton *btn = m_navButtons[index];
    const int y = btn->mapTo(m_sidebar, QPoint(0, 0)).y() + 6;

    m_slider->move(12, y);
    m_slider->show();
    m_slider->raise();   // 盖在导航按钮上方，否则会被按钮背景挡住

    const QRect start = m_slider->geometry();
    const QRect end(12, y, 6, 28);
    m_sliderAnim->stop();
    m_sliderAnim->setStartValue(start);
    m_sliderAnim->setEndValue(end);
    m_sliderAnim->start();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateSliderPosition(m_currentIndex);
    if (m_guide && m_guide->isActive())
        m_guide->relayout();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    {
        QSettings settings;
        settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
        settings.setValue(QStringLiteral("ui/maximized"), isMaximized());
    }
    if (m_forceQuit) {
        event->accept();
        return;
    }

    SettingsManager *sm = SettingsManager::instance();
    if (sm->minimizeToTray() && m_tray) {
        event->ignore();
        hide();
        if (!m_trayHintShown) {
            Toast::showToast(QStringLiteral("已最小化到托盘"),
                             QStringLiteral("双击托盘图标可以重新打开主界面。"),
                             Toast::Info, 4500);
            m_trayHintShown = true;
        }
        return;
    }

    event->accept();
    QApplication::quit();
}

void MainWindow::quitApplication()
{
    m_forceQuit = true;
    if (m_tray)
        m_tray->hide();
    QApplication::quit();
}
