#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>
#include <QPoint>
#include <QString>
#include <QUrl>
#include <QVector>

#include "core/updatemanager.h"

class QFrame;
class SquircleCard;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class UpdateManager;

/**
 * 软件更新窗口（参考「软件更新」版式）：
 *   顶部：图标 + 标题 / 副标题 + 右上角关闭
 *   两张横幅卡片：当前版本 / 更新通道
 *   状态条：发现可用更新 / 已是新版 + 当前版本小药丸
 *   正式版本、预发布版本两张卡片：版本号 + 立即升级 + 可滚动的更新说明
 */
class UpdateDialog : public QDialog
{
    Q_OBJECT

public:
    struct ChannelCard
    {
        SquircleCard *card = nullptr;
        QLabel *title = nullptr;
        QLabel *version = nullptr;
        QLabel *date = nullptr;
        QLabel *state = nullptr;
        QPushButton *action = nullptr;
        QScrollArea *scroll = nullptr;
        QWidget *notesHost = nullptr;
        QVBoxLayout *notesLayout = nullptr;
        UpdateRelease release;
        bool hasRelease = false;
    };

    explicit UpdateDialog(UpdateManager *manager, QWidget *parent = nullptr);

    // 打开窗口并开始检查
    void startCheck();

signals:
    void openedReleasePage(const QUrl &url);
    void checked(const QString &summary);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void buildUi();
    void buildHeader(QVBoxLayout *root);
    void buildBanners(QVBoxLayout *root);
    void buildStatusBar(QVBoxLayout *root);
    ChannelCard buildChannelCard(bool prerelease, QVBoxLayout *root);
    void buildFooter(QVBoxLayout *root);

    void setStatus(const QString &text, int tone);   // 0=普通 1=有更新 2=成功 3=出错
    void applyReleases(const QVector<UpdateRelease> &releases);
    void applyFailure(const QString &error);
    void fillChannel(ChannelCard &card, bool prerelease, const QVector<UpdateRelease> &releases);
    void fillNotes(ChannelCard &card, const QStringList &notes);
    void installFrom(ChannelCard &card);
    void showDemoReleases();

    UpdateManager *m_manager = nullptr;

    QLabel *m_statusText = nullptr;
    QLabel *m_statusChip = nullptr;
    QWidget *m_bellWidget = nullptr;   // BellGlyph，只在 .cpp 里定义
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_pageBtn = nullptr;

    ChannelCard m_stable;
    ChannelCard m_prerelease;

    bool m_dragging = false;
    QPoint m_dragOffset;
};

#endif // UPDATEDIALOG_H
