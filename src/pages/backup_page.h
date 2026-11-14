#ifndef BACKUP_PAGE_H
#define BACKUP_PAGE_H

#include <QDateTime>
#include <QElapsedTimer>
#include <QWidget>

#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>

#include <functional>

#include "core/backupengine.h"

class FlowCanvas;
class QMouseEvent;

/**
 * 文件重命名的胶囊（前缀 / 序号 / 自定义内容）。
 * 只做"显示值 + 左键双击回调"，不支持拖拽，所以不复用 FlowChip。
 */
class RenameChip : public QLabel
{
    Q_OBJECT

public:
    explicit RenameChip(QWidget *parent = nullptr);

    void setActivated(std::function<void()> fn) { m_activated = std::move(fn); }
    void setChipText(const QString &text);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    std::function<void()> m_activated;
};

class BackupPage : public QWidget
{
    Q_OBJECT

public:
    explicit BackupPage(QWidget *parent = nullptr);

    bool isBusy() const;
    void refreshFromSettings();
    QString sourcePath() const;
    QString targetPath() const;

    // 首次引导要高亮的几块
    QWidget *sourceGroupWidget() const { return m_sourceGroup; }
    QWidget *targetGroupWidget() const { return m_targetGroup; }
    QWidget *flowGroupWidget() const { return m_flowGroup; }
    QWidget *flowWidget() const;
    QWidget *renameGroupWidget() const { return m_renameGroup; }
    QWidget *actionBarWidget() const { return m_actionBar; }

public slots:
    void startBackup();

signals:
    void backupCompleted(int files, qint64 bytes, bool verified);

private slots:
    void browseSource();
    void browseTarget();
    void togglePause();
    void stopBackup();
    void onProgress(const BackupStats &stats);
    void onStateChanged(BackupEngine::State state);
    void onError(const QString &message);
    void onCompleted();

private:
    void setupUI();
    QWidget *buildRenameGroup(QWidget *parent);
    void updateButtons();
    void saveSettings();

    // 文件重命名胶囊区
    void refreshRenameChips();
    void updateRenamePreview();
    void refreshPathInfo();

    QLabel *m_sourceInfo = nullptr;
    QComboBox *m_dateSep = nullptr;
    QLabel *m_targetInfo = nullptr;
    void editRenamePrefix();
    void editRenameDigits();
    void editRenameCustom();

    QLineEdit *m_sourceEdit = nullptr;
    QLineEdit *m_targetEdit = nullptr;
    FlowCanvas *m_flow = nullptr;

    QGroupBox *m_sourceGroup = nullptr;
    QGroupBox *m_targetGroup = nullptr;
    QGroupBox *m_flowGroup = nullptr;
    QGroupBox *m_renameGroup = nullptr;
    QWidget *m_actionBar = nullptr;
    QCheckBox *m_renameCheck = nullptr;
    QCheckBox *m_formatCheck = nullptr;
    QCheckBox *m_multithreadCheck = nullptr;

    RenameChip *m_prefixChip = nullptr;
    RenameChip *m_digitsChip = nullptr;
    RenameChip *m_customChip = nullptr;
    QLabel *m_renamePreview = nullptr;

    QProgressBar *m_progressBar = nullptr;
    QLabel *m_speedLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_fileLabel = nullptr;
    QLabel *m_noticeLabel = nullptr;

    QPushButton *m_startBtn = nullptr;
    QPushButton *m_pauseBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;

    BackupEngine *m_engine = nullptr;
    QElapsedTimer m_elapsed;
    QDateTime m_startedAt;
};

#endif // BACKUP_PAGE_H
