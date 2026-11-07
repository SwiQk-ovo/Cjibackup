#include "widgets/squircle.h"
#include "settings_page.h"
#include "core/settingsmanager.h"
#include "core/version.h"
#include "widgets/changelogdialog.h"
#include "widgets/filedialogs.h"
#include "widgets/splashwindow.h"
#include "widgets/typewritertooltip.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace {

QHBoxLayout *rowLayout(QWidget *parent)
{
    auto *layout = new QHBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    return layout;
}

} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

QWidget *SettingsPage::makeRow(const QString &label, QWidget *control, const QString &help)
{
    auto *row = new QWidget(this);
    auto *layout = rowLayout(row);

    auto *name = new QLabel(label, row);
    name->setMinimumWidth(96);
    layout->addWidget(name);
    layout->addWidget(control);

    if (!help.isEmpty())
        layout->addWidget(new HelpIcon(help, row));

    layout->addStretch();
    return row;
}

QWidget *SettingsPage::makeCheckRow(QCheckBox *check, const QString &help)
{
    auto *row = new QWidget(this);
    auto *layout = rowLayout(row);
    layout->addWidget(check);
    if (!help.isEmpty())
        layout->addWidget(new HelpIcon(help, row));
    layout->addStretch();
    return row;
}

void SettingsPage::setupUI()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("pageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("pageContent"));
    scroll->setWidget(content);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(16);

    auto *title = new QLabel("设置", content);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    SettingsManager *sm = SettingsManager::instance();

    /* ---------------- 常规 ---------------- */
    auto *generalGroup = new SquircleGroupBox("常规", content);
    auto *generalLayout = new QVBoxLayout(generalGroup);
    generalLayout->setSpacing(10);

    m_exitCombo = new QComboBox(generalGroup);
    m_exitCombo->addItems({"最小化到系统托盘", "直接退出程序"});
    m_exitCombo->setCurrentIndex(qBound(0, sm->exitBehavior(), 1));
    connect(m_exitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        SettingsManager::instance()->setExitBehavior(index);
    });
    generalLayout->addWidget(makeRow(
        "退出行为", m_exitCombo,
        "关闭窗口时：最小化到托盘＝程序缩到右下角继续运行，定时提醒才会生效；"
        "直接退出＝关掉窗口就结束进程。"));

    m_notifyCheck = new QCheckBox("备份完成时弹出提示", generalGroup);
    m_notifyCheck->setChecked(sm->notifyOnComplete());
    connect(m_notifyCheck, &QCheckBox::toggled, this, [](bool v) {
        SettingsManager::instance()->setNotifyOnComplete(v);
    });
    generalLayout->addWidget(makeCheckRow(
        m_notifyCheck, "备份结束后在屏幕右下角弹出提示卡片，就算主界面最小化了也能看到结果。"));

    m_startMinCheck = new QCheckBox("启动时直接最小化到托盘", generalGroup);
    m_startMinCheck->setChecked(sm->startMinimized());
    connect(m_startMinCheck, &QCheckBox::toggled, this, [](bool v) {
        SettingsManager::instance()->setStartMinimized(v);
    });
    generalLayout->addWidget(makeCheckRow(
        m_startMinCheck, "开机自启或双击快捷方式时不弹主界面，直接驻留托盘，适合常驻。"));

    layout->addWidget(generalGroup);

    /* ---------------- 性能与命名 ---------------- */
    auto *perfGroup = new SquircleGroupBox("性能与命名", content);
    auto *perfLayout = new QVBoxLayout(perfGroup);
    perfLayout->setSpacing(10);

    m_multithreadCheck = new QCheckBox("多线程加速传输", perfGroup);
    m_multithreadCheck->setChecked(sm->multithread());
    connect(m_multithreadCheck, &QCheckBox::toggled, this, [this](bool v) {
        SettingsManager::instance()->setMultithread(v);
        m_threadSpin->setEnabled(v);
    });
    perfLayout->addWidget(makeCheckRow(
        m_multithreadCheck, "同时用多个线程拷贝。SSD / UHS-II 读卡器打开更快；老机械硬盘反而可能变慢。"));

    m_threadSpin = new QSpinBox(perfGroup);
    m_threadSpin->setRange(0, 8);
    m_threadSpin->setSpecialValueText("自动");
    m_threadSpin->setValue(sm->threadCount());
    m_threadSpin->setEnabled(sm->multithread());
    connect(m_threadSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        SettingsManager::instance()->setThreadCount(v);
    });
    perfLayout->addWidget(makeRow(
        "线程数", m_threadSpin, "0 / 自动 表示按 CPU 核心数决定，一般 2~4 个线程就够了。"));

    m_animSlider = new QSlider(Qt::Horizontal, perfGroup);
    m_animSlider->setRange(10, 150);
    m_animSlider->setValue(int(sm->animationSpeed() * 100));
    m_animSlider->setFixedWidth(200);
    m_animValue = new QLabel(QString("%1x").arg(sm->animationSpeed(), 0, 'f', 2), perfGroup);
    m_animValue->setFixedWidth(48);
    connect(m_animSlider, &QSlider::valueChanged, this, [this](int v) {
        const double speed = v / 100.0;
        m_animValue->setText(QString("%1x").arg(speed, 0, 'f', 2));
        SettingsManager::instance()->setAnimationSpeed(speed);
    });
    {
        auto *speedRow = new QWidget(perfGroup);
        auto *speedLayout = rowLayout(speedRow);
        speedLayout->addWidget(m_animSlider);
        speedLayout->addWidget(m_animValue);
        speedLayout->addStretch();
        perfLayout->addWidget(makeRow(
            "动画倍速", speedRow, "界面动画和流程图节点重排的速度，1.0x 是默认手感。"));
    }

    // 重命名规则挪到备份页的胶囊区（v1.1.0），这里只留一句指路
    auto *renameHint = new QLabel(
        "重命名规则在「备份」页设置：流程图下方有三个胶囊（前缀 / 序号 / 自定义内容），"
        "双击即可修改，右侧会实时显示 IMG0001 这样的预览。",
        perfGroup);
    renameHint->setObjectName("hintLabel");
    renameHint->setWordWrap(true);
    perfLayout->addWidget(renameHint);

    layout->addWidget(perfGroup);

    /* ---------------- 备份校验 ---------------- */
    auto *verifyGroup = new SquircleGroupBox("备份校验", content);
    auto *verifyLayout = new QVBoxLayout(verifyGroup);
    verifyLayout->setSpacing(10);

    m_verifyCheck = new QCheckBox("备份完成后校验文件完整性", verifyGroup);
    m_verifyCheck->setChecked(sm->verifyAfterBackup());
    connect(m_verifyCheck, &QCheckBox::toggled, this, [](bool v) {
        SettingsManager::instance()->setVerifyAfterBackup(v);
    });
    verifyLayout->addWidget(makeCheckRow(
        m_verifyCheck, "逐个比对源文件和目标文件的大小，确认没有漏拷、没拷完。"));

    m_formatDialogCheck = new QCheckBox("校验通过后打开 Windows 格式化界面", verifyGroup);
    m_formatDialogCheck->setChecked(sm->openFormatDialog());
    connect(m_formatDialogCheck, &QCheckBox::toggled, this, [](bool v) {
        SettingsManager::instance()->setOpenFormatDialog(v);
    });
    verifyLayout->addWidget(makeCheckRow(
        m_formatDialogCheck,
        "校验全部通过才会拉起 Windows 自带的格式化窗口，是否格式化、点不点开始都由你决定，"
        "软件绝不会自己格式化存储卡。"));

    layout->addWidget(verifyGroup);

    /* ---------------- 启动动画 ---------------- */
    auto *splashGroup = new SquircleGroupBox("启动动画与音效", content);
    auto *splashLayout = new QVBoxLayout(splashGroup);
    splashLayout->setSpacing(10);

    m_splashCheck = new QCheckBox("启用启动动画", splashGroup);
    m_splashCheck->setChecked(sm->splashEnabled());
    connect(m_splashCheck, &QCheckBox::toggled, this, [this](bool v) {
        SettingsManager::instance()->setSplashEnabled(v);
        syncSplashWidgets();
    });
    splashLayout->addWidget(makeCheckRow(
        m_splashCheck, "启动时播放一小段动画（最长 10 秒），可以让开软件更有仪式感。"));

    m_fastStartCheck = new QCheckBox("快速启动模式（跳过动画与音效）", splashGroup);
    m_fastStartCheck->setChecked(sm->fastStart());
    connect(m_fastStartCheck, &QCheckBox::toggled, this, [](bool v) {
        SettingsManager::instance()->setFastStart(v);
    });
    splashLayout->addWidget(makeCheckRow(
        m_fastStartCheck, "勾上之后直接进主界面，不播动画也不放音效。"));
#ifdef CJI_LITE
    splashGroup->hide();   // 便携版没有启动动画，这组设置用不上
#endif

    m_splashPreset = new QComboBox(splashGroup);
    m_splashPreset->setMinimumWidth(220);
    for (const SplashPresets::Item &item : SplashPresets::items())
        m_splashPreset->addItem(item.name, item.key);
    m_splashPreset->addItem("自定义 · 用自己的动图 / 视频", "custom");
    {
        const int index = m_splashPreset->findData(sm->splashPreset());
        m_splashPreset->setCurrentIndex(index < 0 ? 0 : index);
    }
    connect(m_splashPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        SettingsManager::instance()->setSplashPreset(m_splashPreset->currentData().toString());
        syncSplashWidgets();
    });
    splashLayout->addWidget(makeRow(
        "动画风格", m_splashPreset,
        "13 种内置风格：旋涡 / 星尘 / 极光 / 霓虹 / 快门 / 星轨 / 涟漪 / 光爆 / 3D 翻转 / 3D 悬浮 / 3D 切片 / 淡入 / 闪白；也可以选「自定义」用你自己的动图。"));

    m_splashMedia = new QLineEdit(sm->splashMediaPath(), splashGroup);
    m_splashMedia->setPlaceholderText("GIF 动图 或 MP4 / MOV / WebM 视频");
    m_splashMedia->setMinimumWidth(240);
    connect(m_splashMedia, &QLineEdit::textEdited, this, [](const QString &text) {
        SettingsManager::instance()->setSplashMediaPath(text);
    });
    m_splashMediaBtn = new QPushButton("浏览", splashGroup);
    m_splashMediaBtn->setCursor(Qt::PointingHandCursor);
    connect(m_splashMediaBtn, &QPushButton::clicked, this, &SettingsPage::browseSplashMedia);
    {
        auto *mediaRow = new QWidget(splashGroup);
        auto *mediaLayout = rowLayout(mediaRow);
        mediaLayout->addWidget(m_splashMedia);
        mediaLayout->addWidget(m_splashMediaBtn);
        mediaLayout->addStretch();
        splashLayout->addWidget(makeRow(
            "自定义动图 / 视频", mediaRow,
            "支持 GIF 动图，以及 MP4 / MOV / WebM 视频（走 Qt 多媒体模块播放）。"
            "视频会自动循环，启动动画时长到了自动进主界面。"));
    }

    m_splashDuration = new QSpinBox(splashGroup);
    m_splashDuration->setRange(1, 10);
    m_splashDuration->setSuffix(" 秒");
    m_splashDuration->setValue(qMax(1, sm->splashDurationMs() / 1000));
    connect(m_splashDuration, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        SettingsManager::instance()->setSplashDurationMs(v * 1000);
    });
    splashLayout->addWidget(makeRow(
        "动画时长", m_splashDuration, "启动动画最长播放时间，到时间会自动进入主界面。"));

    m_splashSound = new QLineEdit(sm->splashSoundPath(), splashGroup);
    m_splashSound->setPlaceholderText("WAV 音效文件（可留空）");
    m_splashSound->setMinimumWidth(240);
    connect(m_splashSound, &QLineEdit::textEdited, this, [](const QString &text) {
        SettingsManager::instance()->setSplashSoundPath(text);
    });
    m_splashSoundBtn = new QPushButton("浏览", splashGroup);
    m_splashSoundBtn->setCursor(Qt::PointingHandCursor);
    connect(m_splashSoundBtn, &QPushButton::clicked, this, &SettingsPage::browseSplashSound);
    {
        auto *soundRow = new QWidget(splashGroup);
        auto *soundLayout = rowLayout(soundRow);
        soundLayout->addWidget(m_splashSound);
        soundLayout->addWidget(m_splashSoundBtn);
        soundLayout->addStretch();
        splashLayout->addWidget(makeRow(
            "启动音效", soundRow, "只用 WAV，和动画一起播放；留空就是静音启动。"));
    }

    m_soundMode = new QComboBox(splashGroup);
    m_soundMode->addItem("余音 · 让声音放完", "after");
    m_soundMode->addItem("急停 · 动画结束立刻静音", "smooth");
    m_soundMode->addItem("随意 · 不干预", "random");
    {
        const int index = m_soundMode->findData(sm->splashSoundMode());
        m_soundMode->setCurrentIndex(index < 0 ? 0 : index);
    }
    connect(m_soundMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        SettingsManager::instance()->setSplashSoundMode(m_soundMode->currentData().toString());
    });
    splashLayout->addWidget(makeRow(
        "音效模式", m_soundMode,
        "余音＝动画结束后让声音自然放完；急停＝动画一结束立刻静音；随意＝不干预。"));

    auto *previewBtn = new QPushButton("预览启动动画", splashGroup);
    previewBtn->setCursor(Qt::PointingHandCursor);
    connect(previewBtn, &QPushButton::clicked, this, &SettingsPage::previewSplashRequested);
    {
        auto *previewRow = new QWidget(splashGroup);
        auto *previewLayout = rowLayout(previewRow);
        previewLayout->addWidget(previewBtn);
        previewLayout->addStretch();
        splashLayout->addWidget(makeRow(
            "看效果", previewRow, "按当前设置播放一次启动动画（会在屏幕中间弹一下）。"));
    }

    layout->addWidget(splashGroup);

    /* ---------------- 定时提醒 ---------------- */
    auto *reminderGroup = new SquircleGroupBox("定时备份提醒", content);
    auto *reminderLayout = new QVBoxLayout(reminderGroup);
    reminderLayout->setSpacing(10);

    m_reminderCheck = new QCheckBox("到点提醒我备份", reminderGroup);
    m_reminderCheck->setChecked(sm->reminderEnabled());
    connect(m_reminderCheck, &QCheckBox::toggled, this, [this](bool v) {
        SettingsManager::instance()->setReminderEnabled(v);
        saveReminderFromUi();
    });
    reminderLayout->addWidget(makeCheckRow(
        m_reminderCheck, "到点在屏幕右下角弹出提醒卡片，不会自动开始备份。"));

    m_reminderMode = new QComboBox(reminderGroup);
    m_reminderMode->addItem("每日", "daily");
    m_reminderMode->addItem("每周", "weekly");
    m_reminderMode->addItem("每月", "monthly");
    m_reminderMode->addItem("每年", "yearly");
    m_reminderMode->addItem("仅一次", "once");
    {
        const int index = m_reminderMode->findData(sm->reminderMode());
        m_reminderMode->setCurrentIndex(index < 0 ? 0 : index);
    }
    connect(m_reminderMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { saveReminderFromUi(); });
    reminderLayout->addWidget(makeRow(
        "提醒频率", m_reminderMode, "每日 / 每周 / 每月 / 每年按日历重复；仅一次只提醒一次。"));

    m_reminderTime = new QTimeEdit(reminderGroup);
    m_reminderTime->setDisplayFormat("HH:mm");
    m_reminderTime->setTime(QTime::fromString(sm->reminderTime(), "HH:mm"));
    connect(m_reminderTime, &QTimeEdit::timeChanged, this, [this]() { saveReminderFromUi(); });
    reminderLayout->addWidget(makeRow(
        "提醒时间", m_reminderTime, "按本机时间，程序每 30 秒检查一次。"));

    m_reminderWeekday = new QComboBox(reminderGroup);
    m_reminderWeekday->addItems({"周一", "周二", "周三", "周四", "周五", "周六", "周日"});
    m_reminderWeekday->setCurrentIndex(qBound(1, sm->reminderWeekday(), 7) - 1);
    connect(m_reminderWeekday, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this]() { saveReminderFromUi(); });
    reminderLayout->addWidget(makeRow("星期几", m_reminderWeekday, "每周提醒时生效。"));

    m_reminderDate = new QDateEdit(reminderGroup);
    m_reminderDate->setDisplayFormat("yyyy-MM-dd");
    m_reminderDate->setCalendarPopup(true);
    {
        const QDate date = QDate::fromString(sm->reminderDate(), "yyyy-MM-dd");
        m_reminderDate->setDate(date.isValid() ? date : QDate::currentDate());
    }
    connect(m_reminderDate, &QDateEdit::dateChanged, this, [this]() { saveReminderFromUi(); });
    reminderLayout->addWidget(makeRow(
        "提醒日期", m_reminderDate, "仅一次 / 每年提醒时使用。"));

    m_reminderHint = new QLabel(reminderGroup);
    m_reminderHint->setObjectName("hintLabel");
    m_reminderHint->setWordWrap(true);
    reminderLayout->addWidget(m_reminderHint);

    layout->addWidget(reminderGroup);

    /* ---------------- 自动更新 ---------------- */
    auto *updateGroup = new SquircleGroupBox("更新", content);
    auto *updateLayout = new QVBoxLayout(updateGroup);
    updateLayout->setSpacing(10);

    m_autoUpdateCheck = new QCheckBox("启动时自动检查更新", updateGroup);
    m_autoUpdateCheck->setChecked(sm->autoCheckUpdates());
    connect(m_autoUpdateCheck, &QCheckBox::toggled, this, [](bool v) {
        SettingsManager::instance()->setAutoCheckUpdates(v);
    });
    updateLayout->addWidget(makeCheckRow(
        m_autoUpdateCheck, "启动后后台查询一次 GitHub Releases，有更新才提示，不打断你操作。"));

    m_updateChannel = new QComboBox(updateGroup);
    m_updateChannel->addItem("正式版通道", "release");
    m_updateChannel->addItem("测试版通道（含测试版）", "test");
    {
        const int index = m_updateChannel->findData(sm->updateChannel());
        m_updateChannel->setCurrentIndex(index < 0 ? 0 : index);
    }
    connect(m_updateChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        SettingsManager::instance()->setUpdateChannel(m_updateChannel->currentData().toString());
    });
    updateLayout->addWidget(makeRow(
        "更新通道", m_updateChannel,
        "正式版通道只收正式版本；测试版通道能收到带「测试」标记的新版本。"));

    m_checkUpdateBtn = new QPushButton("立即检查更新", updateGroup);
    m_checkUpdateBtn->setObjectName("primaryBtn");
    m_checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &SettingsPage::checkUpdateRequested);

    auto *changelogBtn = new QPushButton("更新日志", updateGroup);
    changelogBtn->setObjectName("ghostBtn");
    changelogBtn->setCursor(Qt::PointingHandCursor);
    changelogBtn->setToolTip("看看这个版本改了什么，也能翻历史版本的更新内容");
    connect(changelogBtn, &QPushButton::clicked, this, [this]() {
        ChangelogDialog::showChangelog(this);
    });

    {
        auto *updateRow = new QWidget(updateGroup);
        auto *updateRowLayout = rowLayout(updateRow);
        m_updateStatus = new QLabel(QString("当前版本：%1").arg(Cji::versionLabel()), updateRow);
        m_updateStatus->setObjectName("hintLabel");
        updateRowLayout->addWidget(m_checkUpdateBtn);
        updateRowLayout->addWidget(changelogBtn);
        updateRowLayout->addWidget(m_updateStatus);
        updateRowLayout->addStretch();
        updateLayout->addWidget(makeRow(
            "检查更新", updateRow, "立即查询一次最新版本，有更新会下载安装包并自动运行。"));
    }

    layout->addWidget(updateGroup);
    layout->addStretch();

    syncSplashWidgets();
    syncReminderWidgets();
}

void SettingsPage::syncSplashWidgets()
{
    SettingsManager *sm = SettingsManager::instance();
    const bool enabled = sm->splashEnabled();
    const bool custom = (m_splashPreset->currentData().toString() == QStringLiteral("custom"));

    m_splashPreset->setEnabled(enabled);
    m_splashDuration->setEnabled(enabled);
    m_splashMedia->setEnabled(enabled && custom);
    m_splashMediaBtn->setEnabled(enabled && custom);
    m_splashSound->setEnabled(enabled);
    m_splashSoundBtn->setEnabled(enabled);
    m_soundMode->setEnabled(enabled);
}

void SettingsPage::syncReminderWidgets()
{
    SettingsManager *sm = SettingsManager::instance();
    const QString mode = m_reminderMode->currentData().toString();
    const bool enabled = m_reminderCheck->isChecked();

    m_reminderMode->setEnabled(enabled);
    m_reminderTime->setEnabled(enabled);
    m_reminderWeekday->setEnabled(enabled && mode == QStringLiteral("weekly"));
    m_reminderDate->setEnabled(enabled
                               && (mode == QStringLiteral("once") || mode == QStringLiteral("yearly")));

    if (!enabled) {
        m_reminderHint->setText(QStringLiteral("提醒已关闭。"));
        return;
    }

    const QString time = sm->reminderTime();
    if (mode == QStringLiteral("daily")) {
        m_reminderHint->setText(QString("每天 %1 提醒一次。").arg(time));
    } else if (mode == QStringLiteral("weekly")) {
        m_reminderHint->setText(QString("每周%1 %2 提醒。").arg(m_reminderWeekday->currentText(), time));
    } else if (mode == QStringLiteral("monthly")) {
        m_reminderHint->setText(QString("每月同日 %1 提醒（短月按最后一天算）。").arg(time));
    } else if (mode == QStringLiteral("yearly")) {
        m_reminderHint->setText(QString("每年 %1 %2 提醒。")
                                    .arg(m_reminderDate->date().toString("MM-dd"), time));
    } else {
        m_reminderHint->setText(QString("仅在 %1 %2 提醒一次。")
                                    .arg(m_reminderDate->date().toString("yyyy-MM-dd"), time));
    }
}

void SettingsPage::saveReminderFromUi()
{
    SettingsManager *sm = SettingsManager::instance();
    sm->setReminderMode(m_reminderMode->currentData().toString());
    sm->setReminderTime(m_reminderTime->time().toString("HH:mm"));
    sm->setReminderWeekday(m_reminderWeekday->currentIndex() + 1);
    sm->setReminderDate(m_reminderDate->date().toString("yyyy-MM-dd"));
    syncReminderWidgets();
}

void SettingsPage::browseSplashMedia()
{
    const QString file = UiDialogs::chooseOpenFile(
        this, "选择启动动画", "动图或视频 (*.gif *.mp4 *.mov *.m4v *.webm *.mkv);;所有文件 (*.*)");
    if (file.isEmpty())
        return;

    m_splashMedia->setText(file);
    SettingsManager::instance()->setSplashMediaPath(file);
    SettingsManager::instance()->setSplashPreset(QStringLiteral("custom"));
    const int index = m_splashPreset->findData(QStringLiteral("custom"));
    if (index >= 0)
        m_splashPreset->setCurrentIndex(index);
    syncSplashWidgets();
}

void SettingsPage::browseSplashSound()
{
    const QString file = UiDialogs::chooseOpenFile(
        this, "选择启动音效", "音频 (*.wav);;所有文件 (*.*)");
    if (file.isEmpty())
        return;

    m_splashSound->setText(file);
    SettingsManager::instance()->setSplashSoundPath(file);
}

void SettingsPage::setUpdateStatus(const QString &text)
{
    if (m_updateStatus)
        m_updateStatus->setText(text);
}
