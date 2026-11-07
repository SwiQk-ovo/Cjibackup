#include "widgets/updatedialog.h"

#include "core/settingsmanager.h"
#include "widgets/squircle.h"
#include "core/version.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace {

const QColor kAccent(59, 130, 246);
const QColor kAmber(251, 191, 36);
const QColor kGreen(52, 211, 153);
const QColor kRed(248, 113, 113);

// 顶部的「上传箭头」图标
class UpdateGlyph : public QWidget
{
public:
    explicit UpdateGlyph(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(44, 44);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setPen(QPen(QColor(0x2F, 0x37, 0x46), 1.0));
        painter.setBrush(QColor(0x1F, 0x24, 0x31));
        painter.drawRoundedRect(box, 12, 12);

        painter.setPen(QPen(QColor(0x60, 0xA5, 0xFA), 2.2, Qt::SolidLine, Qt::RoundCap,
                            Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        const QPointF c = box.center();
        painter.drawLine(QPointF(c.x(), c.y() + 7), QPointF(c.x(), c.y() - 7));
        QPainterPath head;
        head.moveTo(c.x() - 5.5, c.y() - 1.5);
        head.lineTo(c.x(), c.y() - 7.5);
        head.lineTo(c.x() + 5.5, c.y() - 1.5);
        painter.drawPath(head);
    }
};

// 状态条上的小铃铛：有更新时左右摇摆（像真的在响），平时静止
class BellGlyph : public QWidget
{
public:
    explicit BellGlyph(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(20, 20);

        // 一次「响」的节奏：先摆大，再摆小，最后停一下
        m_swing = new QVariantAnimation(this);
        m_swing->setDuration(1700);
        m_swing->setLoopCount(-1);
        m_swing->setKeyValueAt(0.00, 0.0);    // 角度（度）
        m_swing->setKeyValueAt(0.06, -20.0);
        m_swing->setKeyValueAt(0.18, 16.0);
        m_swing->setKeyValueAt(0.30, -12.0);
        m_swing->setKeyValueAt(0.42, 9.0);
        m_swing->setKeyValueAt(0.54, -5.0);
        m_swing->setKeyValueAt(0.66, 2.0);
        m_swing->setKeyValueAt(0.80, 0.0);
        m_swing->setKeyValueAt(1.00, 0.0);
        connect(m_swing, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_angle = value.toReal();
            update();
        });
    }

    void setColor(const QColor &color)
    {
        m_color = color;
        update();
    }

    void setRinging(bool on)
    {
        if (on == m_ringing)
            return;
        m_ringing = on;

        if (on) {
            m_swing->stop();
            m_swing->start();
        } else {
            m_swing->stop();
            m_angle = 0.0;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_color);

        const qreal w = width();
        const qreal h = height();

        // 绕顶部挂点左右摇摆
        painter.translate(w * 0.5, h * 0.12);
        painter.rotate(m_angle);

        QPainterPath bell;
        bell.moveTo(0, h * 0.02);
        bell.cubicTo(-w * 0.26, h * 0.06, -w * 0.22, h * 0.42, -w * 0.25, h * 0.54);
        bell.lineTo(w * 0.25, h * 0.54);
        bell.cubicTo(w * 0.22, h * 0.42, w * 0.26, h * 0.06, 0, h * 0.02);
        bell.closeSubpath();
        painter.drawPath(bell);
        painter.drawEllipse(QPointF(0, h * 0.66), w * 0.08, h * 0.08);
    }

private:
    QColor m_color = kAmber;
    QVariantAnimation *m_swing = nullptr;
    qreal m_angle = 0.0;
    bool m_ringing = false;
};

QString versionText(const QString &version)
{
    return version.isEmpty() ? QStringLiteral("—") : QStringLiteral("v") + version;
}

} // namespace

/* ------------------------------------------------------------------ */

UpdateDialog::UpdateDialog(UpdateManager *manager, QWidget *parent)
    : QDialog(parent)
    , m_manager(manager)
{
    setObjectName(QStringLiteral("updateDialog"));
    setWindowTitle(QStringLiteral("软件更新"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(false);
    resize(720, 640);
    setMinimumSize(600, 520);

    buildUi();

    if (m_manager) {
        connect(m_manager, &UpdateManager::releasesFetched, this,
                &UpdateDialog::applyReleases);
        connect(m_manager, &UpdateManager::releasesFailed, this,
                &UpdateDialog::applyFailure);
        connect(m_manager, &UpdateManager::progress, this,
                [this](qint64 received, qint64 total) {
                    if (total <= 0)
                        return;
                    setStatus(QStringLiteral("正在下载安装包… %1%")
                                  .arg(int(received * 100 / total)),
                              1);
                });
        connect(m_manager, &UpdateManager::installerLaunched, this, [this]() {
            setStatus(QStringLiteral("安装包已启动，按提示完成安装即可。"), 2);
        });
        connect(m_manager, &UpdateManager::failed, this, [this](const QString &error) {
            setStatus(error, 3);
        });
    }
}

void UpdateDialog::buildUi()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *shell = new SquircleCard(this);
    shell->setRadius(16);
    shell->setFill(QColor(0x14, 0x18, 0x21));
    shell->setBorder(QColor(0x2A, 0x2F, 0x3A));
    shell->setObjectName(QStringLiteral("updateShell"));
    outer->addWidget(shell);

    auto *root = new QVBoxLayout(shell);
    root->setContentsMargins(24, 20, 24, 18);
    root->setSpacing(14);

    buildHeader(root);
    buildBanners(root);
    buildStatusBar(root);

    m_stable = buildChannelCard(false, root);
    m_prerelease = buildChannelCard(true, root);

    buildFooter(root);
}

void UpdateDialog::buildHeader(QVBoxLayout *root)
{
    auto *row = new QHBoxLayout();
    row->setSpacing(12);

    row->addWidget(new UpdateGlyph(this), 0, Qt::AlignTop);

    auto *texts = new QVBoxLayout();
    texts->setSpacing(2);

    auto *title = new QLabel(QStringLiteral("软件更新"), this);
    title->setObjectName(QStringLiteral("updateTitle"));
    texts->addWidget(title);

    auto *subtitle = new QLabel(QStringLiteral("获取最新功能与安全性改进"), this);
    subtitle->setObjectName(QStringLiteral("updateSubtitle"));
    texts->addWidget(subtitle);

    row->addLayout(texts);
    row->addStretch(1);

    auto *closeBtn = new QPushButton(QStringLiteral("×"), this);
    closeBtn->setObjectName(QStringLiteral("updateClose"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedSize(30, 30);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    row->addWidget(closeBtn, 0, Qt::AlignTop);

    root->addLayout(row);
}

void UpdateDialog::buildBanners(QVBoxLayout *root)
{
    auto *row = new QHBoxLayout();
    row->setSpacing(12);

    const QString channel = SettingsManager::instance()->updateChannel()
                                    == QStringLiteral("test")
                                ? QStringLiteral("测试版通道")
                                : QStringLiteral("正式版通道");

    auto makeBanner = [this](const QString &caption, const QString &value,
                             const QString &sub, const QString &accent) {
        auto *card = new SquircleCard(this);
        card->setObjectName(QStringLiteral("updateBanner"));
        card->setRadius(12);
        card->setFillGradient(QColor(accent), QColor(0x15, 0x1A, 0x24));
        card->setBorder(QColor(0x26, 0x2C, 0x38));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(14, 10, 14, 10);
        layout->setSpacing(2);

        auto *captionLabel = new QLabel(caption, card);
        captionLabel->setObjectName(QStringLiteral("updateBannerCaption"));
        layout->addWidget(captionLabel);

        auto *valueLabel = new QLabel(value, card);
        valueLabel->setObjectName(QStringLiteral("updateBannerValue"));
        layout->addWidget(valueLabel);

        auto *subLabel = new QLabel(sub, card);
        subLabel->setObjectName(QStringLiteral("updateBannerSub"));
        layout->addWidget(subLabel);

        return card;
    };

    row->addWidget(makeBanner(QStringLiteral("当前版本"),
                              QStringLiteral("v%1").arg(Cji::versionNumber()),
                              Cji::versionLabel(), QStringLiteral("#1B2230")),
                   1);
    row->addWidget(makeBanner(QStringLiteral("更新通道"), channel,
                              QStringLiteral("可在「设置 → 更新」里切换"),
                              QStringLiteral("#14231F")),
                   1);

    root->addLayout(row);
}

void UpdateDialog::buildStatusBar(QVBoxLayout *root)
{
    auto *bar = new SquircleCard(this);
    bar->setObjectName(QStringLiteral("updateStatusBar"));
    bar->setRadius(12);
    bar->setFill(QColor(0x1A, 0x1F, 0x2A));
    bar->setBorder(QColor(0x26, 0x2C, 0x38));
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(16, 10, 16, 10);
    layout->setSpacing(10);

    auto *bell = new BellGlyph(bar);
    m_bellWidget = bell;
    layout->addWidget(bell, 0, Qt::AlignVCenter);

    m_statusText = new QLabel(QStringLiteral("正在检查更新…"), bar);
    m_statusText->setObjectName(QStringLiteral("updateStatusText"));
    layout->addWidget(m_statusText);
    layout->addStretch(1);

    m_statusChip = new QLabel(QStringLiteral("当前: v%1").arg(Cji::versionNumber()), bar);
    m_statusChip->setObjectName(QStringLiteral("updateChip"));
    layout->addWidget(m_statusChip);

    root->addWidget(bar);
}

UpdateDialog::ChannelCard UpdateDialog::buildChannelCard(bool prerelease, QVBoxLayout *root)
{
    ChannelCard card;

    card.card = new SquircleCard(this);
    card.card->setObjectName(QStringLiteral("updateCard"));
    card.card->setRadius(14);
    card.card->setFill(QColor(0x18, 0x1D, 0x27));
    card.card->setBorder(QColor(0x26, 0x2C, 0x38));
    card.card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *layout = new QVBoxLayout(card.card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    auto *head = new QHBoxLayout();
    head->setSpacing(10);

    card.title = new QLabel(prerelease ? QStringLiteral("预发布版本")
                                       : QStringLiteral("正式版本"),
                            card.card);
    card.title->setObjectName(prerelease ? QStringLiteral("updateCardTitlePre")
                                         : QStringLiteral("updateCardTitle"));
    head->addWidget(card.title);

    card.version = new QLabel(QStringLiteral("—"), card.card);
    card.version->setObjectName(QStringLiteral("updateVersion"));
    head->addWidget(card.version);

    card.date = new QLabel(QString(), card.card);
    card.date->setObjectName(QStringLiteral("updateDate"));
    head->addWidget(card.date);

    head->addStretch(1);

    card.state = new QLabel(QStringLiteral("正在检查…"), card.card);
    card.state->setObjectName(QStringLiteral("updateState"));
    head->addWidget(card.state);

    card.action = new QPushButton(QStringLiteral("立即升级"), card.card);
    card.action->setObjectName(QStringLiteral("updateAction"));
    card.action->setCursor(Qt::PointingHandCursor);
    card.action->hide();
    head->addWidget(card.action);

    layout->addLayout(head);

    card.scroll = new QScrollArea(card.card);
    card.scroll->setObjectName(QStringLiteral("updateNotes"));
    card.scroll->setWidgetResizable(true);
    card.scroll->setFrameShape(QFrame::NoFrame);
    card.scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    card.scroll->setMinimumHeight(prerelease ? 132 : 84);

    card.notesHost = new QWidget(card.scroll);
    card.notesHost->setObjectName(QStringLiteral("updateNotesBody"));
    card.notesLayout = new QVBoxLayout(card.notesHost);
    card.notesLayout->setContentsMargins(0, 0, 6, 8);
    card.notesLayout->setSpacing(5);
    card.scroll->setWidget(card.notesHost);

    layout->addWidget(card.scroll);

    connect(card.action, &QPushButton::clicked, this, [this, prerelease]() {
        installFrom(prerelease ? m_prerelease : m_stable);
    });

    root->addWidget(card.card, prerelease ? 2 : 1);
    return card;
}

void UpdateDialog::buildFooter(QVBoxLayout *root)
{
    auto *row = new QHBoxLayout();
    row->setSpacing(10);

    m_refreshBtn = new QPushButton(QStringLiteral("重新检查"), this);
    m_refreshBtn->setObjectName(QStringLiteral("updateGhost"));
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() { startCheck(); });
    row->addWidget(m_refreshBtn);

    m_pageBtn = new QPushButton(QStringLiteral("在 GitHub 上查看"), this);
    m_pageBtn->setObjectName(QStringLiteral("updateGhost"));
    m_pageBtn->setCursor(Qt::PointingHandCursor);
    m_pageBtn->setEnabled(false);
    connect(m_pageBtn, &QPushButton::clicked, this, [this]() {
        const QString url = !m_prerelease.release.pageUrl.isEmpty()
                                ? m_prerelease.release.pageUrl
                                : m_stable.release.pageUrl;
        if (!url.isEmpty())
            QDesktopServices::openUrl(QUrl(url));
    });
    row->addWidget(m_pageBtn);

    row->addStretch(1);

    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    closeBtn->setObjectName(QStringLiteral("updateSecondary"));
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    row->addWidget(closeBtn);

    root->addLayout(row);
}

void UpdateDialog::setStatus(const QString &text, int tone)
{
    if (!m_statusText)
        return;
    m_statusText->setText(text);

    QColor color(0xC9, 0xD1, 0xE0);
    if (tone == 1)
        color = kAmber;
    else if (tone == 2)
        color = kGreen;
    else if (tone == 3)
        color = kRed;
    m_statusText->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));

    // 有更新 / 正在下载时，小铃铛左右摇摆；其他状态让它停下
    if (auto *bell = static_cast<BellGlyph *>(m_bellWidget))
        bell->setRinging(tone == 1);
}

void UpdateDialog::startCheck()
{
    if (!m_manager) {
        applyFailure(QStringLiteral("更新模块没有初始化"));
        return;
    }

    setStatus(QStringLiteral("正在检查更新…"), 0);
    for (ChannelCard *card : {&m_stable, &m_prerelease}) {
        card->state->setText(QStringLiteral("正在检查…"));
        card->state->show();
        card->action->hide();
        card->hasRelease = false;
        card->release = UpdateRelease();
    }

    if (m_refreshBtn)
        m_refreshBtn->setEnabled(false);

    if (qEnvironmentVariableIsSet("CJI_UPDATE_DEMO")) {
        QTimer::singleShot(150, this, [this]() { showDemoReleases(); });
        return;
    }

    m_manager->fetchAllReleases();
}

void UpdateDialog::showDemoReleases()
{
    UpdateRelease stable;
    stable.version = QStringLiteral("1.0.7");
    stable.title = QStringLiteral("拷备 v1.0.7");
    stable.date = QStringLiteral("2026-10-31");
    stable.pageUrl = QStringLiteral("https://github.com/SwiQk-ovo/Cjibackup/releases");
    stable.notes = {
        QStringLiteral("启动动画扩充到 13 套，其中 3 套是真 3D"),
        QStringLiteral("所有消息框改成自绘暗色对话框"),
        QStringLiteral("历史记录支持导出 CSV，Excel 打开中文不乱码"),
    };

    UpdateRelease pre;
    pre.version = QStringLiteral("1.2.0");
    pre.title = QStringLiteral("拷备 v1.2.0");
    pre.date = QStringLiteral("2026-11-08");
    pre.prerelease = true;
    pre.downloadUrl = QStringLiteral("https://example.com/Cjibackup_Setup_1.2.0.exe");
    pre.pageUrl = QStringLiteral("https://github.com/SwiQk-ovo/Cjibackup/releases");
    pre.notes = {
        QStringLiteral("流程图新增「相机型号」节点，按型号自动分文件夹"),
        QStringLiteral("备份页新增「文件重命名」胶囊区，双击即可修改"),
        QStringLiteral("自动识别相机 EXIF 型号：NIKON Z 6_2 显示成 Nikon Z 6II"),
        QStringLiteral("读不到型号的视频、截图归到「未知相机1」"),
        QStringLiteral("备份先算好每个文件的目标路径再拷贝，多线程下序号稳定"),
        QStringLiteral("只在使用「相机型号」节点时才读 EXIF，普通备份不变慢"),
        QStringLiteral("设置页「重命名格式」合并到备份页"),
        QStringLiteral("单文件安装包里可能装不到主程序的问题"),
    };

    applyReleases({pre, stable});
}

void UpdateDialog::applyReleases(const QVector<UpdateRelease> &releases)
{
    if (m_refreshBtn)
        m_refreshBtn->setEnabled(true);

    fillChannel(m_stable, false, releases);
    fillChannel(m_prerelease, true, releases);

    const bool hasPage = !m_stable.release.pageUrl.isEmpty() || !m_prerelease.release.pageUrl.isEmpty();
    if (m_pageBtn)
        m_pageBtn->setEnabled(hasPage);

    const QString current = Cji::versionNumber();
    bool newest = !m_stable.hasRelease || UpdateManager::compareVersions(
                       m_stable.release.version, current) <= 0;
    newest = newest && (!m_prerelease.hasRelease
                        || UpdateManager::compareVersions(m_prerelease.release.version, current) <= 0);

    if (newest) {
        setStatus(QStringLiteral("当前已是最新版本"), 2);
        emit checked(QStringLiteral("当前已是最新版本"));
    } else {
        const QString newer = (m_prerelease.hasRelease
                               && UpdateManager::compareVersions(m_prerelease.release.version, current) > 0)
                                  ? m_prerelease.release.version
                                  : m_stable.release.version;
        setStatus(QStringLiteral("发现可用更新"), 1);
        emit checked(QStringLiteral("发现新版本 v%1").arg(newer));
    }
}

void UpdateDialog::applyFailure(const QString &error)
{
    if (m_refreshBtn)
        m_refreshBtn->setEnabled(true);

    setStatus(error, 3);
    emit checked(error);
    for (ChannelCard *card : {&m_stable, &m_prerelease}) {
        card->state->setText(QStringLiteral("检查失败"));
        card->state->show();
        card->action->hide();
        fillNotes(*card, {QStringLiteral("网络不通或被 GitHub 限流，点左下角「重新检查」再试一次。")});
    }
}

void UpdateDialog::fillChannel(ChannelCard &card, bool prerelease,
                               const QVector<UpdateRelease> &releases)
{
    const UpdateRelease *found = nullptr;
    for (const UpdateRelease &release : releases) {
        if (release.prerelease != prerelease)
            continue;
        found = &release;
        break;
    }

    if (!found) {
        card.hasRelease = false;
        card.version->setText(QStringLiteral("—"));
        card.date->clear();
        card.state->setText(QStringLiteral("暂无"));
        card.state->show();
        card.action->hide();
        fillNotes(card, {QStringLiteral("这个通道还没有发布过版本。")});
        return;
    }

    card.release = *found;
    card.hasRelease = true;
    card.version->setText(versionText(found->version));
    card.date->setText(found->date);

    const int compare = UpdateManager::compareVersions(found->version, Cji::versionNumber());
    if (compare > 0 && !found->downloadUrl.isEmpty()) {
        card.state->hide();
        card.action->show();
        card.action->setEnabled(true);
        card.action->setText(QStringLiteral("立即升级"));
    } else if (compare > 0) {
        card.state->setText(QStringLiteral("有新版本"));
        card.state->show();
        card.action->hide();
    } else {
        card.state->setText(QStringLiteral("无新版本"));
        card.state->show();
        card.action->hide();
    }

    if (found->notes.isEmpty())
        fillNotes(card, {QStringLiteral("这个版本没有写更新说明。")});
    else
        fillNotes(card, found->notes);
}

void UpdateDialog::fillNotes(ChannelCard &card, const QStringList &notes)
{
    if (!card.notesLayout)
        return;

    QLayoutItem *item = nullptr;
    while ((item = card.notesLayout->takeAt(0)) != nullptr) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    for (const QString &note : notes) {
        auto *row = new QWidget(card.notesHost);
        auto *layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        auto *dot = new QLabel(QStringLiteral("✦"), row);
        dot->setObjectName(QStringLiteral("updateNoteDot"));
        dot->setFixedWidth(12);
        dot->setAlignment(Qt::AlignCenter);
        layout->addWidget(dot, 0, Qt::AlignTop);

        auto *text = new QLabel(note, row);
        text->setObjectName(QStringLiteral("updateNote"));
        text->setWordWrap(true);
        layout->addWidget(text, 1);

        card.notesLayout->addWidget(row);
    }

    if (card.scroll)
        card.scroll->verticalScrollBar()->setValue(0);
}

void UpdateDialog::installFrom(ChannelCard &card)
{
    if (!m_manager || !card.hasRelease)
        return;

    card.action->setEnabled(false);
    card.action->setText(QStringLiteral("下载中…"));
    setStatus(QStringLiteral("正在下载 v%1 …").arg(card.release.version), 1);
    m_manager->downloadAndInstall(card.release.downloadUrl, card.release.version);
}

void UpdateDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() < 78) {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void UpdateDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragOffset);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void UpdateDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}
