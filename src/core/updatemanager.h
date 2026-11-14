#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

/** 一条更新说明（按「新增 / 调整 / 修复」分组显示） */
enum class UpdateNoteKind { Added, Changed, Fixed };

struct UpdateNote
{
    UpdateNoteKind kind = UpdateNoteKind::Changed;
    QString text;
};

/** 一条 GitHub Release（更新窗口用） */
struct UpdateRelease
{
    QString version;              // 1.1.0
    QString title;                // Release 标题
    QString date;                 // 2026-11-01
    QVector<UpdateNote> notes;    // 拆好的一条条更新说明（带分类）
    QString downloadUrl;          // 安装包地址
    QString pageUrl;              // Release 页面
    bool prerelease = false;
};

/**
 * 自动更新：查 GitHub Releases -> 比较版本 -> 下载安装包 -> 运行安装包。
 * Token 不写死在代码里，只从环境变量 CJI_GITHUB_TOKEN 读取（可选）。
 */
class UpdateManager : public QObject
{
    Q_OBJECT

public:
    explicit UpdateManager(QObject *parent = nullptr);

    void checkForUpdates();
    // 更新窗口用：拉全部版本（正式版 + 预发布），交给窗口自己分栏显示
    void fetchAllReleases();
    void downloadAndInstall(const QString &url, const QString &version);

    // "测试 v0.3.0" / "v0.3.0" / "0.3" -> "0.3.0"
    static QString normalizeVersion(const QString &raw);
    static int compareVersions(const QString &a, const QString &b);
    // Release 说明 -> 一条条纯文本，并按「新增 / 调整 / 修复」分好类
    static QVector<UpdateNote> parseReleaseNotes(const QString &raw);

signals:
    void statusChanged(const QString &text);
    void updateFound(const QString &version, const QString &notes,
                     const QString &downloadUrl, const QString &pageUrl);
    void upToDate(const QString &currentVersion);
    void failed(const QString &error);
    void progress(qint64 received, qint64 total);
    void installerLaunched();

    void releasesFetched(const QVector<UpdateRelease> &releases);
    void releasesFailed(const QString &error);

private:
    void handleCheckReply(QNetworkReply *reply);
    void handleListReply(QNetworkReply *reply);
    void abortCheck();

    QNetworkAccessManager *m_net = nullptr;
    QNetworkReply *m_checkReply = nullptr;
    QNetworkReply *m_listReply = nullptr;
    QNetworkReply *m_downloadReply = nullptr;
    QFile *m_downloadFile = nullptr;
    QString m_downloadPath;
    bool m_busy = false;
    bool m_listBusy = false;
};

#endif // UPDATEMANAGER_H
