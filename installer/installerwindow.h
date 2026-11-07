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
