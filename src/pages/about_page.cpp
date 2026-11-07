#include "widgets/squircle.h"
#include "about_page.h"
#include "core/appdirs.h"
#include "core/appicon.h"
#include "core/version.h"
#include "widgets/changelogdialog.h"
#include "widgets/typewritertooltip.h"

#include "widgets/toast.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace {

// 看上去像超链接（悬停变亮 + 下划线），点了走回调，而不是打开网页
class ClickableLabel : public QLabel
{
public:
    using QLabel::QLabel;

    std::function<void()> onClick;

protected:
    void enterEvent(QEnterEvent *event) override
    {
        QLabel::enterEvent(event);
        QFont f = font();
        f.setUnderline(true);
        setFont(f);
        setStyleSheet(QStringLiteral("color:#93C5FD;"));
    }

    void leaveEvent(QEvent *event) override
    {
        QLabel::leaveEvent(event);
        QFont f = font();
        f.setUnderline(false);
        setFont(f);
        setStyleSheet(QStringLiteral("color:#3B82F6;"));
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && onClick)
            onClick();
        QLabel::mouseReleaseEvent(event);
    }
};

} // namespace

AboutPage::AboutPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *logo = new QLabel(this);
    logo->setPixmap(AppIcon::logoPixmap(96));
    logo->setAlignment(Qt::AlignCenter);
    layout->addWidget(logo);

    auto *name = new QLabel("KOPAL", this);
    name->setObjectName("aboutLogo");
    name->setAlignment(Qt::AlignCenter);
    layout->addWidget(name);


    auto *version = new QLabel(Cji::versionLabel(), this);
    version->setAlignment(Qt::AlignCenter);
    version->setObjectName("versionLabel");
    layout->addWidget(version);

    {
        auto *changelogRow = new QHBoxLayout();
        changelogRow->setAlignment(Qt::AlignCenter);
        changelogRow->setSpacing(8);

        auto *changelogBtn = new QPushButton("更新日志", this);
        changelogBtn->setObjectName("ghostBtn");
        changelogBtn->setCursor(Qt::PointingHandCursor);
        changelogBtn->setToolTip("看看这个版本改了什么，也能翻历史版本的更新内容");
        connect(changelogBtn, &QPushButton::clicked, this, [this]() {
            ChangelogDialog::showChangelog(this);
        });
        changelogRow->addWidget(changelogBtn);

        m_guideBtn = new QPushButton("使用引导", this);
        m_guideBtn->setObjectName("ghostBtn");
        m_guideBtn->setCursor(Qt::PointingHandCursor);
        m_guideBtn->setToolTip("再把每个功能的位置和用法过一遍");
        connect(m_guideBtn, &QPushButton::clicked, this, [this]() {
            emit guideRequested();
        });
        changelogRow->addWidget(m_guideBtn);

        layout->addLayout(changelogRow);
    }

    auto *tagline = new QLabel("把相机存储卡里的照片，封存成琥珀", this);
    tagline->setObjectName("hintLabel");
    tagline->setAlignment(Qt::AlignCenter);
    layout->addWidget(tagline);

    layout->addSpacing(10);

    auto *author = new QLabel("作者：适诠SwiQk_", this);
    author->setAlignment(Qt::AlignCenter);
    layout->addWidget(author);

    auto *links = new QHBoxLayout();
    links->setAlignment(Qt::AlignCenter);

    auto *bili = new QLabel(
        "<a href='https://space.bilibili.com/3493116898183194' style='color:#3B82F6;'>B站主页</a>",
        this);
    bili->setOpenExternalLinks(true);
    links->addWidget(bili);

    auto *dot = new QLabel(" · ", this);
    dot->setObjectName("hintLabel");
    links->addWidget(dot);

    auto *github = new QLabel(
        "<a href='https://github.com/SwiQk-ovo/Cjibackup' style='color:#3B82F6;'>GitHub 仓库</a>",
        this);
    github->setOpenExternalLinks(true);
    links->addWidget(github);

    layout->addLayout(links);

    // Bug 反馈群：点一下就复制群号
    auto *bugGroup = new ClickableLabel(QStringLiteral("Bug 反馈群：1135151552（点击复制）"), this);
    bugGroup->setObjectName(QStringLiteral("aboutLink"));
    bugGroup->setAlignment(Qt::AlignCenter);
    bugGroup->setCursor(Qt::PointingHandCursor);
    bugGroup->setToolTip(QStringLiteral("左键单击复制群号"));
    bugGroup->onClick = []() {
        QGuiApplication::clipboard()->setText(QStringLiteral("1135151552"));
        Toast::showToast(QStringLiteral("已复制"),
                         QStringLiteral("Bug 反馈群号 1135151552 已复制到剪贴板，在 QQ 里搜索就能进群"),
                         Toast::Success, 3000);
    };
    layout->addWidget(bugGroup);

    auto *dataDir = new QLabel(
        QString("数据目录：%1").arg(Cji::dataDir()), this);
    dataDir->setObjectName("hintLabel");
    dataDir->setAlignment(Qt::AlignCenter);
    layout->addWidget(dataDir);

    layout->addStretch();

    auto *copyright = new QLabel("© 2026 适诠SwiQk_ · 备份前请确认存储卡已插入并可以被系统读取", this);
    copyright->setObjectName("hintLabel");
    copyright->setAlignment(Qt::AlignCenter);
    layout->addWidget(copyright);
}
