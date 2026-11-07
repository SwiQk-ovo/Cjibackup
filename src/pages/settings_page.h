#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimeEdit;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setUpdateStatus(const QString &text);

signals:
    void checkUpdateRequested();
    void previewSplashRequested();

private:
    void setupUI();

    QWidget *makeRow(const QString &label, QWidget *control, const QString &help);
    QWidget *makeCheckRow(QCheckBox *check, const QString &help);
    void syncReminderWidgets();
    void saveReminderFromUi();
    void browseSplashMedia();
    void browseSplashSound();
    void syncSplashWidgets();

    QComboBox *m_exitCombo = nullptr;
    QCheckBox *m_notifyCheck = nullptr;
    QCheckBox *m_startMinCheck = nullptr;

    QCheckBox *m_multithreadCheck = nullptr;
    QSpinBox *m_threadSpin = nullptr;
    QSlider *m_animSlider = nullptr;
    QLabel *m_animValue = nullptr;

    QCheckBox *m_verifyCheck = nullptr;
    QCheckBox *m_formatDialogCheck = nullptr;

    QCheckBox *m_splashCheck = nullptr;
    QCheckBox *m_fastStartCheck = nullptr;
    QComboBox *m_splashPreset = nullptr;
    QLineEdit *m_splashMedia = nullptr;
    QPushButton *m_splashMediaBtn = nullptr;
    QSpinBox *m_splashDuration = nullptr;
    QLineEdit *m_splashSound = nullptr;
    QPushButton *m_splashSoundBtn = nullptr;
    QComboBox *m_soundMode = nullptr;

    QCheckBox *m_reminderCheck = nullptr;
    QComboBox *m_reminderMode = nullptr;
    QTimeEdit *m_reminderTime = nullptr;
    QComboBox *m_reminderWeekday = nullptr;
    QDateEdit *m_reminderDate = nullptr;
    QLabel *m_reminderHint = nullptr;

    QCheckBox *m_autoUpdateCheck = nullptr;
    QComboBox *m_updateChannel = nullptr;
    QPushButton *m_checkUpdateBtn = nullptr;
    QLabel *m_updateStatus = nullptr;
};

#endif // SETTINGS_PAGE_H
