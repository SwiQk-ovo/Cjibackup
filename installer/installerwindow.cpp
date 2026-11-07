#include "installerwindow.h"

#include "installdata.h"
#include "widgets/darkdialog.h"
#include "widgets/filedialogs.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QTextStream>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dwmapi.h>
#endif

namespace {

void applyDarkTitleBar(QWidget *window)
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
#else
    Q_UNUSED(window)
#endif
}

void installLog(const QString &message)
{
    QFile file(QDir::tempPath() + "/cji_installer.log");
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString("hh:mm:ss") << " " << message << "\n";
    }
}



QPixmap loadLogo(int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF body(2, 2, size - 4, size - 4);
    QLinearGradient gradient(body.topLeft(), body.bottomRight());
    gradient.setColorAt(0.0, QColor(96, 165, 250));
    gradient.setColorAt(0.55, QColor(59, 130, 246));
    gradient.setColorAt(1.0, QColor(29, 78, 216));
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawRoundedRect(body, size * 0.26, size * 0.26);

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(int(size * 0.56));
    font.setFamily(QStringLiteral("Microsoft YaHei UI"));
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(body, Qt::AlignCenter, QStringLiteral("C"));
    painter.end();
    return pixmap;
}

} // namespace

InstallerWindow::InstallerWindow(bool testMode, QWidget *parent)
    : QWidget(parent)
    , m_testMode(testMode)
{
    setWindowTitle(QStringLiteral("拷备 安装程序"));
    setWindowIcon(QIcon(loadLogo(64)));
    setFixedSize(720, 480);
    applyDarkTitleBar(this);

    QString error;
    if (!m_payload.open(QCoreApplication::applicationFilePath(), &error)) {
        m_testMode = true;   // 数据读不到时不允许真的写系统
        buildUi();
        if (m_footerStatus) {
            m_footerStatus->setText(error);
            m_footerStatus->setStyleSheet(QStringLiteral("color:#F87171;"));
        }
        if (m_nextBtn)
            m_nextBtn->setEnabled(false);
        return;
    }

    buildUi();
}

void InstallerWindow::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 头部
    auto *header = new QFrame(this);
    header->setObjectName("installerHeader");
    header->setFixedHeight(96);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 16, 28, 16);
    headerLayout->setSpacing(16);

    auto *logo = new QLabel(header);
    logo->setPixmap(loadLogo(56));
    logo->setFixedSize(56, 56);
    headerLayout->addWidget(logo);

    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("拷备"), header);
    title->setObjectName("installerTitle");
    auto *subtitle = new QLabel(
        QStringLiteral("相机存储卡备份工具 · %1").arg(InstallData::VersionLabel), header);
    subtitle->setObjectName("installerSubtitle");
    titleBox->addWidget(title);
    titleBox->addWidget(subtitle);
    titleBox->addStretch();
    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();

    m_headerStatus = new QLabel(QStringLiteral("安装向导"), header);
    m_headerStatus->setObjectName("installerStep");
    headerLayout->addWidget(m_headerStatus);

    root->addWidget(header);

    // 页面
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildWelcomePage());
    m_pages->addWidget(buildPathPage());
    m_pages->addWidget(buildProgressPage());
    m_pages->addWidget(buildDonePage());
    root->addWidget(m_pages, 1);

    // 底部
    auto *footer = new QFrame(this);
    footer->setObjectName("installerFooter");
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 12, 24, 12);
    footerLayout->setSpacing(10);

    m_footerStatus = new QLabel(QStringLiteral("建议先关闭正在运行的程序"), footer);
    m_footerStatus->setObjectName("installerHint");
    footerLayout->addWidget(m_footerStatus);
    footerLayout->addStretch();

    m_cancelBtn = new QPushButton(QStringLiteral("取消"), footer);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QWidget::close);
    footerLayout->addWidget(m_cancelBtn);

    m_backBtn = new QPushButton(QStringLiteral("上一步"), footer);
    m_backBtn->setCursor(Qt::PointingHandCursor);
    connect(m_backBtn, &QPushButton::clicked, this, &InstallerWindow::previousPage);
    footerLayout->addWidget(m_backBtn);

    m_nextBtn = new QPushButton(QStringLiteral("下一步"), footer);
    m_nextBtn->setObjectName("installerPrimary");
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    connect(m_nextBtn, &QPushButton::clicked, this, &InstallerWindow::nextPage);
    footerLayout->addWidget(m_nextBtn);

    root->addWidget(footer);

    m_pathEdit->setText(InstallData::defaultInstallDir());
    updateButtons();
}

QWidget *InstallerWindow::buildWelcomePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 32, 48, 24);
    layout->setSpacing(14);

    auto *headline = new QLabel(QStringLiteral("欢迎安装 拷备"), page);
    headline->setObjectName("installerHeadline");
    layout->addWidget(headline);

    auto *intro = new QLabel(page);
    intro->setObjectName("installerBody");
    intro->setWordWrap(true);
    intro->setText(
        QStringLiteral("安装程序会把 拷备 %1 安装到你的电脑上，并创建桌面 / 开始菜单快捷方式，"
                       "可以在「设置 - 应用」里随时卸载。")
            .arg(InstallData::VersionLabel));
    layout->addWidget(intro);

    auto *points = new QLabel(page);
    points->setObjectName("installerBody");
    points->setWordWrap(true);
    points->setText(
        QStringLiteral("· 支持按流程图把 DCIM 里的照片视频分到年 / 月 / 日 / 事件等文件夹\n"
                       "· 备份完成会校验文件完整性，校验通过才会问你要不要格式化存储卡\n"
                       "· 关掉窗口会缩到托盘，可以设定时提醒，到点用系统通知叫你备份\n"
                       "· 会自动检查新版本，有更新时提示你下载安装"));
    layout->addWidget(points);

    auto *note = new QLabel(
        QStringLiteral("提示：安装过程不会动你的照片和存储卡，所有操作都在你点确认之后才进行。"), page);
    note->setObjectName("installerHint");
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

QWidget *InstallerWindow::buildPathPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 32, 48, 24);
    layout->setSpacing(14);

    auto *headline = new QLabel(QStringLiteral("选择安装位置"), page);
    headline->setObjectName("installerHeadline");
    layout->addWidget(headline);

    auto *hint = new QLabel(
        QStringLiteral("默认安装在 D:\\Cjibackup。如果你想装到别的地方，选一个文件夹即可，"
                       "安装程序会自动在后面补上 Cjibackup 子文件夹。"),
        page);
    hint->setObjectName("installerBody");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *row = new QHBoxLayout();
    m_pathEdit = new QLineEdit(page);
    m_pathEdit->setMinimumHeight(36);
    auto *browse = new QPushButton(QStringLiteral("浏览…"), page);
    browse->setCursor(Qt::PointingHandCursor);
    connect(browse, &QPushButton::clicked, this, &InstallerWindow::browseInstallDir);
    row->addWidget(m_pathEdit, 1);
    row->addWidget(browse);
    layout->addLayout(row);

    auto *space = new QLabel(page);
    space->setObjectName("installerHint");
    layout->addWidget(space);

    m_desktopCheck = new QCheckBox(QStringLiteral("创建桌面快捷方式"), page);
    m_desktopCheck->setChecked(true);
    layout->addWidget(m_desktopCheck);

    auto *startMenu = new QLabel(QStringLiteral("开始菜单快捷方式会一并创建。"), page);
    startMenu->setObjectName("installerHint");
    layout->addWidget(startMenu);
    layout->addStretch();
    return page;
}

QWidget *InstallerWindow::buildProgressPage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 48, 48, 24);
    layout->setSpacing(16);

    auto *headline = new QLabel(QStringLiteral("正在安装…"), page);
    headline->setObjectName("installerHeadline");
    layout->addWidget(headline);

    m_progressBar = new QProgressBar(page);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    layout->addWidget(m_progressBar);

    m_progressLabel = new QLabel(QStringLiteral("准备中…"), page);
    m_progressLabel->setObjectName("installerHint");
    m_progressLabel->setWordWrap(true);
    layout->addWidget(m_progressLabel);
    layout->addStretch();
    return page;
}

QWidget *InstallerWindow::buildDonePage()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(48, 40, 48, 24);
    layout->setSpacing(14);

    auto *headline = new QLabel(QStringLiteral("安装完成"), page);
    headline->setObjectName("installerHeadline");
    layout->addWidget(headline);

    m_doneLabel = new QLabel(page);
    m_doneLabel->setObjectName("installerBody");
    m_doneLabel->setWordWrap(true);
    layout->addWidget(m_doneLabel);

    m_runCheck = new QCheckBox(QStringLiteral("立即运行 拷备"), page);
    m_runCheck->setChecked(true);
    layout->addWidget(m_runCheck);

    auto *tip = new QLabel(
        QStringLiteral("第一次启动会播放一小段启动动画，可以在「设置」里关掉或改成快速启动。"), page);
    tip->setObjectName("installerHint");
    tip->setWordWrap(true);
    layout->addWidget(tip);
    layout->addStretch();
    return page;
}

void InstallerWindow::updateButtons()
{
    const int index = m_pages->currentIndex();
    const bool installing = (index == 2);
    const bool done = (index == 3);

    m_backBtn->setVisible(index == 1);
    m_nextBtn->setVisible(!done);
    m_cancelBtn->setVisible(!installing && !done);
    m_nextBtn->setText(index == 1 ? QStringLiteral("安装") : QStringLiteral("下一步"));
    m_nextBtn->setEnabled(!installing && m_payload.isOpen());

    if (m_headerStatus) {
        switch (index) {
        case 0: m_headerStatus->setText(QStringLiteral("第 1 / 3 步 · 欢迎")); break;
        case 1: m_headerStatus->setText(QStringLiteral("第 2 / 3 步 · 选择位置")); break;
        case 2: m_headerStatus->setText(QStringLiteral("第 3 / 3 步 · 安装中")); break;
        default: m_headerStatus->setText(QStringLiteral("完成")); break;
        }
    }

    if (done) {
        m_nextBtn->setVisible(true);
        m_nextBtn->setText(QStringLiteral("完成"));
        m_nextBtn->setEnabled(true);
    }
}

void InstallerWindow::nextPage()
{
    const int index = m_pages->currentIndex();
    if (index == 0) {
        m_pages->setCurrentIndex(1);
        updateButtons();
        return;
    }
    if (index == 1) {
        startInstall();
        return;
    }
    if (index == 3) {
        finish();
        return;
    }
}

void InstallerWindow::previousPage()
{
    if (m_pages->currentIndex() == 1) {
        m_pages->setCurrentIndex(0);
        updateButtons();
    }
}

void InstallerWindow::browseInstallDir()
{
    const QString dir = UiDialogs::chooseDirectory(
        this, QStringLiteral("选择安装位置"), m_pathEdit->text());
    if (!dir.isEmpty())
        m_pathEdit->setText(dir);
}

void InstallerWindow::setInitialDir(const QString &dir)
{
    if (!dir.trimmed().isEmpty() && m_pathEdit)
        m_pathEdit->setText(InstallData::normalizeInstallDir(dir));
}

bool InstallerWindow::performInstall(const QString &dir, bool createShortcuts,
                                     const std::function<void(int, const QString &)> &progress,
                                     QString *error)
{
    m_installDir = dir;
    installLog(QString("performInstall dir=%1 payloadOpen=%2 entries=%3")
                   .arg(dir).arg(m_payload.isOpen()).arg(m_payload.count()));

    if (!QDir().mkpath(m_installDir)) {
        if (error)
            *error = QStringLiteral("无法创建安装目录：\n%1").arg(m_installDir);
        return false;
    }

    QFile probe(m_installDir + QStringLiteral("/.cji_write_test"));
    if (!probe.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("这个位置没有写入权限，请换一个目录：\n%1").arg(m_installDir);
        return false;
    }
    probe.close();
    QFile::remove(probe.fileName());

    const int total = qMax(1, m_payload.count());
    const bool extracted = m_payload.extractAll(
        m_installDir,
        [&progress, total](int index, const QString &path) {
            if (progress)
                progress(int(double(index + 1) * 100.0 / total), path);
            QCoreApplication::processEvents();
        },
        error);

    if (!extracted)
    {
        installLog(QString("extract failed: %1").arg(error ? *error : QString()));
        return false;
    }

    if (createShortcuts && !m_testMode) {
        QString shortcutError;
        InstallData::createShortcuts(m_installDir,
                                     m_desktopCheck ? m_desktopCheck->isChecked() : true,
                                     &shortcutError);
        InstallData::writeUninstallEntry(m_installDir);
    }

    if (progress)
        progress(100, QStringLiteral("完成"));
    return true;
}

bool InstallerWindow::runSilent()
{
    if (!m_payload.isOpen())
        return false;

    const QString dir = m_pathEdit && !m_pathEdit->text().trimmed().isEmpty()
                            ? InstallData::normalizeInstallDir(m_pathEdit->text())
                            : InstallData::defaultInstallDir();

    QString error;
    const bool ok = performInstall(dir, true, nullptr, &error);
    installLog(QString("runSilent ok=%1 err=%2 dir=%3").arg(ok).arg(error).arg(dir));
    return ok;
}

void InstallerWindow::startInstall()
{
    const QString chosen = m_pathEdit->text().trimmed();
    if (chosen.isEmpty()) {
        DarkDialog::warning(this, "提示", "请先选择安装位置");
        return;
    }

    m_pages->setCurrentIndex(2);
    updateButtons();
    m_progressBar->setValue(0);
    m_progressLabel->setText(QStringLiteral("正在准备安装…"));
    QCoreApplication::processEvents();

    QString error;
    const bool ok = performInstall(
        InstallData::normalizeInstallDir(chosen), true,
        [this](int percent, const QString &path) {
            m_progressBar->setValue(percent);
            m_progressLabel->setText(QStringLiteral("正在安装：%1").arg(path));
            QCoreApplication::processEvents();
        },
        &error);

    if (!ok) {
        DarkDialog::warning(this, "安装失败", error);
        m_pages->setCurrentIndex(1);
        updateButtons();
        return;
    }

    m_installed = true;
    m_doneLabel->setText(
        QString("拷备 %1 已安装到：\n%2\n\n%3")
            .arg(InstallData::VersionLabel, QDir::toNativeSeparators(m_installDir),
                 m_testMode ? QStringLiteral("（测试模式：没有创建快捷方式和卸载项）")
                            : QStringLiteral("桌面和开始菜单快捷方式已经创建好，可以在「设置 - 应用」中卸载。")));

    m_pages->setCurrentIndex(3);
    updateButtons();
}
void InstallerWindow::finish()
{
    if (m_installed && m_runCheck->isChecked()) {
        const QString exe = m_installDir + QLatin1Char('/') + InstallData::AppExeName;
        QProcess::startDetached(exe, QStringList(), m_installDir);
    }
    close();
}
