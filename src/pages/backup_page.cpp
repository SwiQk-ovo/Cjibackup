#include "widgets/squircle.h"
#include "backup_page.h"
#include "core/cameranaming.h"
#include "core/historymanager.h"
#include "core/settingsmanager.h"
#include "core/winutils.h"
#include "core/exifscanner.h"
#include "widgets/darkdialog.h"
#include "widgets/filedialogs.h"
#include "widgets/toast.h"
#include "core/flowpath.h"
#include "widgets/flowcanvas.h"

#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QStorageInfo>
#include <QTime>
#include <QVBoxLayout>

#include <functional>

namespace {

QString localHumanSize(qint64 bytes)
{
    const double kb = 1024.0;
    if (bytes < kb) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < kb * kb) return QStringLiteral("%1 KB").arg(bytes / kb, 0, 'f', 0);
    if (bytes < kb * kb * kb) return QStringLiteral("%1 MB").arg(bytes / (kb * kb), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (kb * kb * kb), 0, 'f', 1);
}

} // namespace
namespace {

QString digitsHint(int digits)
{
    return QStringLiteral("%1 位").arg(qBound(1, digits, 8));
}

} // namespace

/* ------------------------------------------------------------------ */
/* RenameChip                                                          */
/* ------------------------------------------------------------------ */

RenameChip::RenameChip(QWidget *parent)
    : QLabel(parent)
{
    setObjectName(QStringLiteral("flowChip"));
    setAlignment(Qt::AlignCenter);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(30);
    setStyleSheet(QStringLiteral("padding-left: 12px; padding-right: 12px;"));
}

void RenameChip::setChipText(const QString &text)
{
    setText(text);
    const int width = fontMetrics().horizontalAdvance(text) + 28;
    setFixedWidth(qMax(64, width));
}

void RenameChip::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_activated)
            m_activated();
        event->accept();
        return;
    }
    QLabel::mouseDoubleClickEvent(event);
}

BackupPage::BackupPage(QWidget *parent)
    : QWidget(parent)
    , m_engine(new BackupEngine(this))
{
    setupUI();
    refreshFromSettings();

    connect(m_engine, &BackupEngine::progressUpdated, this, &BackupPage::onProgress);
    connect(m_engine, &BackupEngine::stateChanged, this, &BackupPage::onStateChanged);
    connect(m_engine, &BackupEngine::errorOccurred, this, &BackupPage::onError);
    connect(m_engine, &BackupEngine::completed, this, &BackupPage::onCompleted);

    // 流程图变化立刻保存
    connect(m_flow, &FlowCanvas::flowChanged, this, [](const QStringList &config) {
        SettingsManager::instance()->setFlowConfig(config);
    });

    updateButtons();
}

void BackupPage::refreshFromSettings()
{
    SettingsManager *sm = SettingsManager::instance();
    m_sourceEdit->setText(sm->sourcePath());
    m_targetEdit->setText(sm->targetPath());
    m_renameCheck->setChecked(sm->autoRename());
    m_formatCheck->setChecked(sm->formatAfterBackup());
    m_multithreadCheck->setChecked(sm->multithread());
    m_flow->setFlowConfig(sm->flowConfig());
    refreshRenameChips();
}

QWidget *BackupPage::buildRenameGroup(QWidget *parent)
{
    auto *group = new SquircleGroupBox("文件重命名（双击胶囊修改）", parent);
    m_renameGroup = group;
    auto *layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    auto *hint = new QLabel(
        "三个胶囊拼出文件名：前缀 + 补零序号 + 自定义内容，例如 IMG0001婚礼.jpg；"
        "序号按目标文件夹分别从 1 开始。",
        group);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *row = new QHBoxLayout();
    row->setSpacing(8);

    auto *prefixChip = new RenameChip(group);
    prefixChip->setToolTip(QStringLiteral("双击修改前缀（文件名开头的固定文字），默认 IMG"));
    prefixChip->setActivated([this]() { editRenamePrefix(); });
    m_prefixChip = prefixChip;
    row->addWidget(prefixChip);

    auto *digitsChip = new RenameChip(group);
    digitsChip->setToolTip(QStringLiteral("双击修改序号位数（3/4/5/6），比如 4 位 → 0001"));
    digitsChip->setActivated([this]() { editRenameDigits(); });
    m_digitsChip = digitsChip;
    row->addWidget(digitsChip);

    auto *customChip = new RenameChip(group);
    customChip->setToolTip(QStringLiteral("双击添加 / 修改文件名末尾的自定义内容，比如 婚礼"));
    customChip->setActivated([this]() { editRenameCustom(); });
    m_customChip = customChip;
    row->addWidget(customChip);

    row->addStretch(1);

    m_renamePreview = new QLabel(group);
    m_renamePreview->setObjectName("flowPreview");
    m_renamePreview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(m_renamePreview);

    layout->addLayout(row);
    return group;
}

void BackupPage::refreshRenameChips()
{
    SettingsManager *sm = SettingsManager::instance();

    const QString prefix = sm->renamePrefix().trimmed();
    m_prefixChip->setChipText(
        QStringLiteral("前缀 %1").arg(prefix.isEmpty() ? QStringLiteral("（空）") : prefix));

    m_digitsChip->setChipText(QStringLiteral("序号 %1").arg(digitsHint(sm->renameDigits())));

    const QString custom = sm->renameCustom().trimmed();
    m_customChip->setChipText(
        QStringLiteral("自定义 %1").arg(custom.isEmpty() ? QStringLiteral("（空）") : custom));

    updateRenamePreview();
}

// 源/目标信息：文件数、总大小、目标盘剩余空间与是否装得下
void BackupPage::refreshPathInfo()
{
    const QString src = m_sourceEdit->text().trimmed();
    const QString dst = m_targetEdit->text().trimmed();

    if (src.isEmpty()) {
        m_sourceInfo->setText(QStringLiteral("尚未选择源文件夹"));
    } else if (!QDir(src).exists()) {
        m_sourceInfo->setText(QStringLiteral("源文件夹不存在"));
    } else {
        qint64 totalBytes = 0;
        int fileCount = 0;
        QDirIterator it(src, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++fileCount;
            totalBytes += it.fileInfo().size();
            if (fileCount > 200000)
                break;
        }
        m_sourceInfo->setText(QStringLiteral("识别到 %1 个文件 · 共 %2")
                                  .arg(fileCount).arg(localHumanSize(totalBytes)));

        if (!dst.isEmpty()) {
            QStorageInfo storage(dst);
            if (storage.isValid() && storage.isReady()) {
                const qint64 freeBytes = storage.bytesAvailable();
                const bool enough = freeBytes > totalBytes + 64LL * 1024 * 1024;
                m_targetInfo->setText(
                    QStringLiteral("目标盘剩余 %1 · 需要 %2 · %3")
                        .arg(localHumanSize(freeBytes), localHumanSize(totalBytes),
                             enough ? QStringLiteral("空间充足")
                                    : QStringLiteral("空间不足，请清理或换盘")));
                return;
            }
        }
    }

    if (dst.isEmpty()) {
        m_targetInfo->setText(QStringLiteral("尚未选择目标文件夹"));
    } else {
        QStorageInfo storage(dst);
        m_targetInfo->setText(storage.isValid() && storage.isReady()
                                  ? QStringLiteral("目标盘剩余 %1").arg(localHumanSize(storage.bytesAvailable()))
                                  : QStringLiteral("目标路径不可用"));
    }
}

void BackupPage::updateRenamePreview()
{
    if (!m_renamePreview)
        return;

    SettingsManager *sm = SettingsManager::instance();
    NamingPlan plan;
    plan.prefix = sm->renamePrefix();
    plan.digits = sm->renameDigits();
    plan.custom = sm->renameCustom();

    const QString sample = plan.build(1) + QStringLiteral(".jpg");
    if (m_renameCheck && !m_renameCheck->isChecked()) {
        m_renamePreview->setText(
            QStringLiteral("预览：%1（勾选「自动重命名文件」后生效）").arg(sample));
    } else {
        m_renamePreview->setText(QStringLiteral("预览：%1").arg(sample));
    }
}

void BackupPage::editRenamePrefix()
{
    SettingsManager *sm = SettingsManager::instance();
    bool ok = false;
    const QString value = QInputDialog::getText(
        this, QStringLiteral("重命名 · 前缀"),
        QStringLiteral("文件名开头的固定文字（可留空）:"),
        QLineEdit::Normal, sm->renamePrefix(), &ok);
    if (!ok)
        return;

    sm->setRenamePrefix(value.trimmed());
    refreshRenameChips();
}

void BackupPage::editRenameDigits()
{
    SettingsManager *sm = SettingsManager::instance();
    const QStringList options{
        QStringLiteral("3 位（001）"),
        QStringLiteral("4 位（0001）"),
        QStringLiteral("5 位（00001）"),
        QStringLiteral("6 位（000001）")
    };

    int index = qBound(0, sm->renameDigits() - 3, options.size() - 1);
    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, QStringLiteral("重命名 · 序号"),
        QStringLiteral("序号占几位（不足会自动补 0）:"),
        options, index, false, &ok);
    if (!ok)
        return;

    const int picked = options.indexOf(chosen);
    if (picked < 0)
        return;

    sm->setRenameDigits(picked + 3);
    refreshRenameChips();
}

void BackupPage::editRenameCustom()
{
    SettingsManager *sm = SettingsManager::instance();
    bool ok = false;
    const QString value = QInputDialog::getText(
        this, QStringLiteral("重命名 · 自定义内容"),
        QStringLiteral("文件名末尾追加的内容（可留空）:"),
        QLineEdit::Normal, sm->renameCustom(), &ok);
    if (!ok)
        return;

    sm->setRenameCustom(value.trimmed());
    refreshRenameChips();
}

QString BackupPage::sourcePath() const
{
    return m_sourceEdit->text().trimmed();
}

QString BackupPage::targetPath() const
{
    return m_targetEdit->text().trimmed();
}

QWidget *BackupPage::flowWidget() const
{
    return m_flow;
}

bool BackupPage::isBusy() const
{
    const BackupEngine::State state = m_engine->state();
    return state == BackupEngine::Running || state == BackupEngine::Paused;
}

void BackupPage::setupUI()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("pageScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    content->setObjectName("pageContent");
    scroll->setWidget(content);

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(16);

    // 标题
    auto *title = new QLabel("备份", content);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // 源文件夹
    auto *srcGroup = new SquircleGroupBox("源文件夹（相机 DCIM）", content);
    m_sourceGroup = srcGroup;
    auto *srcOuter = new QVBoxLayout(srcGroup);
    srcOuter->setSpacing(6);
    auto *srcLayout = new QHBoxLayout();
    m_sourceEdit = new QLineEdit(srcGroup);
    m_sourceEdit->setPlaceholderText("选择相机存储卡的 DCIM 文件夹，会递归备份里面的所有照片 / 视频");
    auto *srcBtn = new QPushButton("浏览", srcGroup);
    srcBtn->setCursor(Qt::PointingHandCursor);
    connect(srcBtn, &QPushButton::clicked, this, &BackupPage::browseSource);
    srcLayout->addWidget(m_sourceEdit);
    srcLayout->addWidget(srcBtn);
    srcOuter->addLayout(srcLayout);
    m_sourceInfo = new QLabel(srcGroup);
    m_sourceInfo->setObjectName("hintLabel");
    srcOuter->addWidget(m_sourceInfo);
    layout->addWidget(srcGroup);

    // 目标文件夹
    auto *dstGroup = new SquircleGroupBox("目标文件夹（本地磁盘）", content);
    m_targetGroup = dstGroup;
    auto *dstOuter = new QVBoxLayout(dstGroup);
    dstOuter->setSpacing(6);
    auto *dstLayout = new QHBoxLayout();
    m_targetEdit = new QLineEdit(dstGroup);
    m_targetEdit->setPlaceholderText("例如 D:\\Cjibackup");
    auto *dstBtn = new QPushButton("浏览", dstGroup);
    dstBtn->setCursor(Qt::PointingHandCursor);
    connect(dstBtn, &QPushButton::clicked, this, &BackupPage::browseTarget);
    dstLayout->addWidget(m_targetEdit);
    dstLayout->addWidget(dstBtn);
    dstOuter->addLayout(dstLayout);
    m_targetInfo = new QLabel(dstGroup);
    m_targetInfo->setObjectName("hintLabel");
    dstOuter->addWidget(m_targetInfo);
    layout->addWidget(dstGroup);

    connect(m_sourceEdit, &QLineEdit::textChanged, this, [this]() { refreshPathInfo(); });
    connect(m_targetEdit, &QLineEdit::textChanged, this, [this]() { refreshPathInfo(); });

    // 分类流程图
    auto *flowGroup = new SquircleGroupBox("分类流程图（把分类项拖进画布，从左到右决定文件夹层级）", content);
    m_flowGroup = flowGroup;
    auto *flowLayout = new QVBoxLayout(flowGroup);
    flowLayout->setSpacing(8);

    auto *flowHint = new QLabel(
        "把上方分类项拖进画布（也可双击直接添加），从左到右依次生成多级文件夹；"
        "把一个节点拖到另一个节点上可以合并成一层文件夹，双击合并节点能改分隔符；"
        "右键拖动两个节点可以改连线，只有从左到右有效，未连线的分类项不会生成文件夹。",
        flowGroup);
    flowHint->setObjectName("hintLabel");
    flowHint->setWordWrap(true);
    flowLayout->addWidget(flowHint);

    // 日期分隔符：年月日合并成一个文件夹时用，可改 . - _ 空格 或自定义
    {
        auto *sepRow = new QHBoxLayout();
        auto *sepLabel = new QLabel("日期分隔符", flowGroup);
        sepLabel->setObjectName("settingLabel");
        auto *sepCombo = new QComboBox(flowGroup);
        sepCombo->setObjectName(QStringLiteral("dateSep"));
        sepCombo->addItem("- 连字符", "-");
        sepCombo->addItem(". 点", ".");
        sepCombo->addItem("_ 下划线", "_");
        sepCombo->addItem("空格", " ");
        m_dateSep = sepCombo;
        const QString cur = SettingsManager::instance()->pathSeparator();
        const int idx = sepCombo->findData(cur);
        sepCombo->setCurrentIndex(idx < 0 ? 0 : idx);
        FlowPath::setSeparator(cur);
        connect(sepCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, sepCombo]() {
            const QString sep = sepCombo->currentData().toString();
            SettingsManager::instance()->setPathSeparator(sep);
            FlowPath::setSeparator(sep);
            if (m_flow)
                m_flow->refreshPreview();
            updateRenamePreview();
        });
        sepRow->addWidget(sepLabel);
        sepRow->addWidget(sepCombo);
        sepRow->addStretch();
        flowLayout->addLayout(sepRow);
    }
    m_flow = new FlowCanvas(flowGroup);
    flowLayout->addWidget(m_flow);

    layout->addWidget(flowGroup);

    // 文件重命名胶囊区（流程图下方、其他选项上方）
    layout->addWidget(buildRenameGroup(content));

    // 其他选项
    auto *optGroup = new SquircleGroupBox("其他选项", content);
    auto *optLayout = new QVBoxLayout(optGroup);

    m_renameCheck = new QCheckBox(QStringLiteral("自动重命名文件"), optGroup);
    m_renameCheck->setToolTip(QStringLiteral("前缀 / 序号 / 自定义内容可以在上面的胶囊里双击修改"));
    connect(m_renameCheck, &QCheckBox::toggled, this, [this](bool) { updateRenamePreview(); });
    optLayout->addWidget(m_renameCheck);

    m_formatCheck = new QCheckBox("备份完成后格式化存储卡", optGroup);
    optLayout->addWidget(m_formatCheck);

    m_multithreadCheck = new QCheckBox("多线程加速传输", optGroup);
    optLayout->addWidget(m_multithreadCheck);

    layout->addWidget(optGroup);

    // 进度
    auto *progGroup = new SquircleGroupBox("传输进度", content);
    auto *progLayout = new QVBoxLayout(progGroup);

    m_progressBar = new QProgressBar(progGroup);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    progLayout->addWidget(m_progressBar);

    auto *infoLayout = new QHBoxLayout();
    m_speedLabel = new QLabel("速度: --", progGroup);
    m_timeLabel = new QLabel("用时: --", progGroup);
    m_fileLabel = new QLabel("文件: 0/0", progGroup);
    infoLayout->addWidget(m_speedLabel);
    infoLayout->addWidget(m_timeLabel);
    infoLayout->addWidget(m_fileLabel);
    infoLayout->addStretch();
    progLayout->addLayout(infoLayout);

    m_noticeLabel = new QLabel(progGroup);
    m_noticeLabel->setObjectName("noticeLabel");
    m_noticeLabel->setWordWrap(true);
    progLayout->addWidget(m_noticeLabel);

    layout->addWidget(progGroup);

    // 按钮
    m_actionBar = new QWidget(content);
    auto *btnLayout = new QHBoxLayout(m_actionBar);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    m_startBtn = new QPushButton("开始传输", content);
    m_startBtn->setObjectName("primaryBtn");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_pauseBtn = new QPushButton("暂停", content);
    m_pauseBtn->setCursor(Qt::PointingHandCursor);
    m_pauseBtn->setEnabled(false);
    m_stopBtn = new QPushButton("终止", content);
    m_stopBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn->setEnabled(false);

    connect(m_startBtn, &QPushButton::clicked, this, &BackupPage::startBackup);
    connect(m_pauseBtn, &QPushButton::clicked, this, &BackupPage::togglePause);
    connect(m_stopBtn, &QPushButton::clicked, this, &BackupPage::stopBackup);

    btnLayout->addStretch();
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_pauseBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addStretch();
    layout->addWidget(m_actionBar);

    layout->addStretch();
}

void BackupPage::browseSource()
{
    const QString dir = UiDialogs::chooseDirectory(this, "选择 DCIM 文件夹", m_sourceEdit->text());
    if (dir.isEmpty())
        return;
    m_sourceEdit->setText(dir);
    SettingsManager::instance()->setSourcePath(dir);

    // 选完目录立刻扫一遍 EXIF，告诉用户卡里有几台相机
    m_noticeLabel->setText("正在识别相机型号…");
    const QVector<CameraInfo> cameras = ExifScanner::scanDirectory(dir, 400);
    if (cameras.isEmpty()) {
        m_noticeLabel->setText("没有读到相机型号：可能是视频或截图，或这张卡不是相机格式");
        return;
    }

    QStringList names;
    int sampled = 0;
    for (const CameraInfo &camera : cameras) {
        names << QString("%1（%2 张）").arg(camera.model).arg(camera.files);
        sampled += camera.files;
    }
    m_flow->setCameraModelHint(cameras.first().model);
    m_noticeLabel->setText(QString("检测到 %1 台相机：%2　·　共采样 %3 个文件")
                               .arg(cameras.size())
                               .arg(names.join("、"))
                               .arg(sampled));
}

void BackupPage::browseTarget()
{
    const QString dir = UiDialogs::chooseDirectory(this, "选择目标文件夹", m_targetEdit->text());
    if (dir.isEmpty())
        return;
    m_targetEdit->setText(dir);
    SettingsManager::instance()->setTargetPath(dir);
}

void BackupPage::saveSettings()
{
    SettingsManager *sm = SettingsManager::instance();
    sm->setSourcePath(m_sourceEdit->text().trimmed());
    sm->setTargetPath(m_targetEdit->text().trimmed());
    sm->setFlowConfig(m_flow->flowConfig());
    sm->setAutoRename(m_renameCheck->isChecked());
    sm->setFormatAfterBackup(m_formatCheck->isChecked());
    sm->setMultithread(m_multithreadCheck->isChecked());
}

void BackupPage::startBackup()
{
    if (isBusy())
        return;

    const QString src = m_sourceEdit->text().trimmed();
    const QString dst = m_targetEdit->text().trimmed();

    if (src.isEmpty() || dst.isEmpty()) {
        DarkDialog::warning(this, "提示", "请先选择源文件夹和目标文件夹");
        return;
    }
    if (!QDir(src).exists()) {
        DarkDialog::warning(this, "提示", "源文件夹不存在，请重新选择");
        return;
    }
    if (m_flow->flowConfig().isEmpty()
        && !DarkDialog::question(this, "提示",
                                 "流程画布是空的，所有文件会直接放进目标文件夹。\n确定继续吗？",
                                 "继续备份", "再想想")) {
        return;
    }

    saveSettings();

    m_progressBar->setValue(0);
    m_noticeLabel->clear();
    m_elapsed.start();
    m_startedAt = QDateTime::currentDateTime();

    m_engine->start(src, dst);
    updateButtons();
}

void BackupPage::togglePause()
{
    if (m_engine->state() == BackupEngine::Running)
        m_engine->pause();
    else if (m_engine->state() == BackupEngine::Paused)
        m_engine->resume();
}

void BackupPage::stopBackup()
{
    if (!DarkDialog::confirmTwice(this, QStringLiteral("终止备份"),
                                  QStringLiteral("现在中断的话，已经拷过去的文件会保留，没拷完的不会继续。\n"
                                                 "确定要终止当前备份吗？"),
                                  QStringLiteral("终止备份"))) {
        return;
    }

    m_engine->stop();
    Toast::showToast(QStringLiteral("已终止备份"),
                     QStringLiteral("已经拷过去的文件会留在目标文件夹里"), Toast::Warning, 8000);
}

void BackupPage::onProgress(const BackupStats &stats)
{
    if (stats.totalBytes > 0)
        m_progressBar->setValue(int(stats.copiedBytes * 100 / stats.totalBytes));

    m_fileLabel->setText(QString("文件: %1/%2").arg(stats.copiedFiles).arg(stats.totalFiles));
    m_speedLabel->setText(QString("速度: %1 MB/s").arg(stats.speedMBps, 0, 'f', 1));

    const qint64 seconds = m_elapsed.elapsed() / 1000;
    m_timeLabel->setText(QString("用时: %1")
                             .arg(QTime(0, 0).addSecs(int(seconds)).toString("hh:mm:ss")));

    if (!stats.currentFile.isEmpty())
        m_noticeLabel->setText(QString("正在复制：%1").arg(stats.currentFile));
}

void BackupPage::onStateChanged(BackupEngine::State)
{
    updateButtons();
}

void BackupPage::onError(const QString &message)
{
    m_noticeLabel->setText(message);

    // 还没开始就失败（空间不够 / 目录建不了 / 目标不合法）：直接弹框，别只写在角落
    if (m_engine->state() == BackupEngine::Idle)
        DarkDialog::warning(this, "无法开始备份", message);
}

void BackupPage::onCompleted()
{
    const BackupStats stats = m_engine->stats();
    const VerifyResult verify = m_engine->verifyResult();
    const QString src = m_sourceEdit->text().trimmed();
    const QString dst = m_targetEdit->text().trimmed();
    const int elapsedSecs = int(m_elapsed.elapsed() / 1000);

    bool verifiedOk = true;

    // 一个文件都没拷进去、而且有失败记录：这是复制阶段就挂了，别拿"补拷"糊弄用户
    if (stats.copiedFiles == 0 && stats.failedCount > 0) {
        const QStringList failed = m_engine->failedFiles();
        QString detail = failed.mid(0, 3).join(QStringLiteral("\n"));
        if (failed.size() > 3)
            detail += QStringLiteral("\n…共 %1 个").arg(failed.size());
        m_noticeLabel->setText(
            QString("复制失败：%1 个文件一个都没进去").arg(stats.failedCount));
        DarkDialog::critical(this, "复制失败",
                             QString("有 %1 个文件复制失败，一个都没写进目标文件夹。\n\n"
                                     "常见原因：目标盘没空间、没有写入权限、文件被杀毒软件占用。\n\n"
                                     "失败的文件：\n%2")
                                 .arg(stats.failedCount)
                                 .arg(detail));
        return;
    }

    if (verify.ran) {
        verifiedOk = (verify.missing == 0 && verify.corrupt == 0);
        if (verifiedOk) {
            m_noticeLabel->setText(
                QString("校验通过（SHA-256）：%1 个文件内容一致").arg(verify.checked));
        } else {
            m_noticeLabel->setText(
                QString("校验发现 %1 个缺失、%2 个内容不一致")
                    .arg(verify.missing).arg(verify.corrupt));
            const bool retry = stats.copiedFiles > 0 && DarkDialog::question(
                this, "校验未通过",
                QString("有 %1 个文件没有完整拷贝到目标目录。\n"
                        "要现在自动补拷一遍吗？（已存在的文件会自动跳过）")
                    .arg(verify.missing),
                "现在补拷", "先跳过");
            if (retry) {
                m_elapsed.restart();
                m_engine->start(src, dst);
                updateButtons();
                return;
            }
        }
    }

    HistoryRecord record;
    record.startTime = m_startedAt;
    record.endTime = QDateTime::currentDateTime();
    record.fileCount = stats.copiedFiles;
    record.totalBytes = stats.totalBytes;
    record.sourcePath = src;
    record.targetPath = dst;
    record.avgSpeed = elapsedSecs > 0 ? double(stats.totalBytes) / 1024.0 / 1024.0 / elapsedSecs : 0;
    record.formatted = false;

    if (m_formatCheck->isChecked() && verifiedOk
        && SettingsManager::instance()->openFormatDialog()) {
        const bool openDialog = DarkDialog::confirmTwice(
            this, QStringLiteral("校验通过"),
            QStringLiteral("全部文件已校验完整。\n\n"
                           "要现在打开 %1 的 Windows 格式化界面吗？\n"
                           "是否格式化由你在系统窗口里点确认，软件不会替你格式化。")
                .arg(WinUtils::driveDescription(src)),
            QStringLiteral("打开格式化界面"));
        if (openDialog) {
            const int ret = WinUtils::openFormatDialog(src, this);
            if (ret == -99)
                m_noticeLabel->setText(QStringLiteral("该路径不是本地磁盘，已跳过格式化"));
            else
                record.formatted = (ret == 0);
        }
    }

    HistoryManager::instance()->addRecord(record);
    emit backupCompleted(stats.copiedFiles, stats.totalBytes, verifiedOk);
}

void BackupPage::updateButtons()
{
    const BackupEngine::State s = m_engine->state();
    const bool running = (s == BackupEngine::Running || s == BackupEngine::Paused);

    m_startBtn->setEnabled(!running);
    m_pauseBtn->setEnabled(running);
    m_stopBtn->setEnabled(running);
    m_pauseBtn->setText(s == BackupEngine::Paused ? "继续" : "暂停");
}
