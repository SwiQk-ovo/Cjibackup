#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPropertyAnimation>

class QGraphicsOpacityEffect;
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

class AlbumPage;
class BackupPage;
class SettingsPage;
class HistoryPage;
class EventsPage;
class AboutPage;
class GuideOverlay;
class SplashWindow;
class UpdateDialog;
class QAction;
class QTimer;
class QMenu;
class QSystemTrayIcon;
class ReminderManager;
class UpdateManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void prepareShow();
    void showFromTray();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void switchPage(int index);
    void quitApplication();
    void onBackupCompleted(int files, qint64 bytes, bool verified);
    void onReminderTriggered(const QDateTime &due);
    void checkForUpdates(bool manual);
    void onUpdateFound(const QString &version, const QString &notes,
                       const QString &downloadUrl, const QString &pageUrl);

private:
    void setupUI();
    void setupSidebar();
    void setupPages();
    void setupTray();
    void setupReminder();
    void setupUpdater();
    void updateSliderPosition(int index);
    void startGuide();
    void openUpdateWindow(bool startCheck);

    QWidget *m_centralWidget = nullptr;
    QHBoxLayout *m_mainLayout = nullptr;
    QFrame *m_sidebar = nullptr;
    QVBoxLayout *m_sidebarLayout = nullptr;
    QStackedWidget *m_pages = nullptr;
    QPushButton *m_navButtons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    QLabel *m_logoLabel = nullptr;
    QFrame *m_slider = nullptr;
    QGraphicsOpacityEffect *m_pageFade = nullptr;
    QPropertyAnimation *m_pageFadeAnim = nullptr;
    QPropertyAnimation *m_sliderAnim = nullptr;
    int m_currentIndex = 0;

    BackupPage *m_backupPage = nullptr;
    AlbumPage *m_albumPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    HistoryPage *m_historyPage = nullptr;
    EventsPage *m_eventsPage = nullptr;
    AboutPage *m_aboutPage = nullptr;
    GuideOverlay *m_guide = nullptr;
    SplashWindow *m_splashPreview = nullptr;
    UpdateDialog *m_updateDialog = nullptr;
    QTimer *m_updatePoll = nullptr;      // 已经打开时定时查更新
    QString m_notifiedVersion;           // 已经提示过的版本，避免重复弹

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_startAction = nullptr;
    ReminderManager *m_reminder = nullptr;
    UpdateManager *m_updater = nullptr;

    bool m_manualUpdateCheck = false;
    bool m_trayHintShown = false;
    bool m_forceQuit = false;
    bool m_hasSavedGeometry = false;   // 有没有恢复过上次的窗口几何
};

#endif // MAINWINDOW_H
