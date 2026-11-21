#ifndef INSTALLERWINDOW_H
#define INSTALLERWINDOW_H

#include <QWidget>

#include <functional>

#include "payload.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;

class InstallerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit InstallerWindow(bool testMode, QWidget *parent = nullptr);

    void setInitialDir(const QString &dir);
    // 自动更新用：--exact 时不再自动补 KOPAL 子目录，原地覆盖
    void setExactDir(bool exact) { m_exactDir = exact; }
    // 自动更新用：等旧进程退出、静默安装、装完自动重启
    void setAutoRestart(bool on) { m_autoRestart = on; }
    // 便携版自动更新：不写快捷方式和卸载注册表
    void setNoShortcuts(bool on) { m_noShortcuts = on; }
    // 无界面安装（自动更新 / 命令行用），成功返回 true
    bool runSilent();

private:
    void buildUi();
    QWidget *buildWelcomePage();
    QWidget *buildPathPage();
    QWidget *buildProgressPage();
    QWidget *buildDonePage();
    void updateButtons();
    void nextPage();
    void previousPage();
    void startInstall();
    void browseInstallDir();
    void finish();

    bool performInstall(const QString &dir, bool createShortcuts,
                        const std::function<void(int, const QString &)> &progress, QString *error);

    bool m_testMode = false;
    bool m_exactDir = false;
    bool m_autoRestart = false;
    bool m_noShortcuts = false;
    bool m_installed = false;
    PayloadReader m_payload;
    QString m_installDir;

    QStackedWidget *m_pages = nullptr;
    QLabel *m_headerStatus = nullptr;
    QLabel *m_footerStatus = nullptr;

    QLineEdit *m_pathEdit = nullptr;
    QCheckBox *m_desktopCheck = nullptr;

    QProgressBar *m_progressBar = nullptr;
    QLabel *m_progressLabel = nullptr;

    QLabel *m_doneLabel = nullptr;
    QCheckBox *m_runCheck = nullptr;

    QPushButton *m_cancelBtn = nullptr;
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
};

#endif // INSTALLERWINDOW_H
