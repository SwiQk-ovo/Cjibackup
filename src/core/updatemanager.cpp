#include "updatemanager.h"

#include "settingsmanager.h"
#include "version.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace {

const QString kReleasesApi =
    QStringLiteral("https://api.github.com/repos/SwiQk-ovo/Cjibackup/releases?per_page=20");
const QString kRepoUrl = QStringLiteral("https://github.com/SwiQk-ovo/Cjibackup");

QString authToken()
{
    // 不硬编码 Token；要访问私有仓库时把 CJI_GITHUB_TOKEN 设到环境变量即可
    return qEnvironmentVariable("CJI_GITHUB_TOKEN");
}

// GitHub 上的发布说明如果本身存成了乱码，显示出来就是一串问号；
// 这种情况不把乱码丢给用户，改成一句人话 + 浏览器查看原文。
QString cleanReleaseNotes(const QString &raw)
{
    QString text = raw;
    text.remove(QChar(0xFFFD));

    int questions = 0;
    int meaningful = 0;
    for (const QChar &ch : text) {
        if (ch == QLatin1Char('?'))
            ++questions;
        if (!ch.isSpace())
            ++meaningful;
    }

    const bool broken = meaningful > 8 && questions * 100 / qMax(1, meaningful) > 25;
    if (broken) {
        return QStringLiteral("这个版本的更新说明在 GitHub 上存成了乱码，"
                              "可以点下面的「在浏览器里查看」看原文。");
    }

    text = text.trimmed();
    if (text.size() > 500)
        text = text.left(500) + QStringLiteral("…");
    if (text.isEmpty())
        return QStringLiteral("这个版本没有写更新说明。");
    return text;
}

} // namespace

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QStringList UpdateManager::parseReleaseNotes(const QString &raw)
{
    QStringList lines;
    const QStringList rawLines = raw.split(QRegularExpression(QStringLiteral("[\r\n]+")));

    static const QRegularExpression bullet(QStringLiteral("^\\s*(?:[-*+·•]|\\d+[.)])\\s*"));
    static const QRegularExpression heading(QStringLiteral("^\\s{0,3}#{1,6}\\s*"));
    static const QRegularExpression markup(QStringLiteral("[`*_]{1,3}"));

    for (QString line : rawLines) {
        line.replace(QChar(0xFFFD), QLatin1Char(' '));
        line.remove(heading);
        line.remove(bullet);
        line.remove(markup);
        line = line.simplified();

        if (line.isEmpty())
            continue;
        if (line.startsWith(QStringLiteral("---")) || line.startsWith(QStringLiteral("===")))
            continue;
        // 纯链接行 / 图片行没意义，跳过
        if (line.startsWith(QStringLiteral("![")) || line.startsWith(QStringLiteral("http")))
            continue;

        if (line.size() > 120)
            line = line.left(120) + QStringLiteral("…");
        lines << line;
        if (lines.size() >= 40)
            break;
    }
    return lines;
}

QString UpdateManager::normalizeVersion(const QString &raw)
{
    static const QRegularExpression pattern(QStringLiteral("(\\d+(?:\\.\\d+)*)"));
    const QRegularExpressionMatch match = pattern.match(raw);
    if (!match.hasMatch())
        return QString();

    QStringList parts = match.captured(1).split('.');
    while (parts.size() < 3)
        parts << QStringLiteral("0");
    return parts.mid(0, 3).join('.');
}

int UpdateManager::compareVersions(const QString &a, const QString &b)
{
    const QString left = normalizeVersion(a);
    const QString right = normalizeVersion(b);
    if (left.isEmpty() || right.isEmpty())
        return left.compare(right);

    const QStringList ls = left.split('.');
    const QStringList rs = right.split('.');

    for (int i = 0; i < 3; ++i) {
        const int l = i < ls.size() ? ls.at(i).toInt() : 0;
        const int r = i < rs.size() ? rs.at(i).toInt() : 0;
        if (l != r)
            return l < r ? -1 : 1;
    }
    return 0;
}

void UpdateManager::abortCheck()
{
    if (m_checkReply) {
        m_checkReply->disconnect(this);
        m_checkReply->abort();
        m_checkReply->deleteLater();
        m_checkReply = nullptr;
    }
}

void UpdateManager::checkForUpdates()
{
    if (m_busy) {
        emit statusChanged(QStringLiteral("正在检查中…"));
        return;
    }

    abortCheck();
    m_busy = true;
    emit statusChanged(QStringLiteral("正在检查更新…"));

    QNetworkRequest request{QUrl(kReleasesApi)};
    request.setRawHeader("User-Agent", "Cjibackup-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);

    const QString token = authToken();
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    m_checkReply = m_net->get(request);
    connect(m_checkReply, &QNetworkReply::finished, this,
            [this]() { handleCheckReply(m_checkReply); });
}

void UpdateManager::handleCheckReply(QNetworkReply *reply)
{
    m_busy = false;
    if (!reply)
        return;

    m_checkReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404)
            emit failed(QStringLiteral("GitHub 上还没有发布版本"));
        else
            emit failed(QStringLiteral("检查更新失败：%1").arg(reply->errorString()));
        return;
    }

    const QByteArray payload = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        emit failed(QStringLiteral("GitHub 返回的数据看不懂（可能触发限流）"));
        return;
    }

    SettingsManager *sm = SettingsManager::instance();
    const bool allowTest = sm->updateChannel() == QStringLiteral("test");
    const QString current = Cji::versionNumber();

    QString bestVersion;
    QString bestNotes;
    QString bestDownload;
    QString bestPage;

    for (const QJsonValue &value : doc.array()) {
        const QJsonObject release = value.toObject();
        if (release.value("draft").toBool())
            continue;
        if (release.value("prerelease").toBool() && !allowTest)
            continue;

        const QString tag = release.value("tag_name").toString();
        const QString version = normalizeVersion(tag);
        if (version.isEmpty())
            continue;
        if (compareVersions(version, current) <= 0)
            continue;
        if (!bestVersion.isEmpty() && compareVersions(version, bestVersion) <= 0)
            continue;

        QString download;
        for (const QJsonValue &assetValue : release.value("assets").toArray()) {
            const QJsonObject asset = assetValue.toObject();
            const QString name = asset.value("name").toString();
            if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
                download = asset.value("browser_download_url").toString();
                if (name.contains(QStringLiteral("安装")) || name.contains(QStringLiteral("Setup"))
                    || name.contains(QStringLiteral("Installer")))
                    break;
            }
        }

        bestVersion = version;
        bestNotes = cleanReleaseNotes(release.value("body").toString());
        bestDownload = download;
        bestPage = release.value("html_url").toString();
    }

    if (bestVersion.isEmpty()) {
        emit statusChanged(QStringLiteral("当前已是最新版本：%1").arg(Cji::versionLabel()));
        emit upToDate(current);
        return;
    }

    emit statusChanged(QStringLiteral("发现新版本：v%1").arg(bestVersion));
    emit updateFound(bestVersion, bestNotes, bestDownload, bestPage);
}

void UpdateManager::fetchAllReleases()
{
    if (m_listBusy) {
        emit statusChanged(QStringLiteral("正在检查中…"));
        return;
    }

    if (m_listReply) {
        m_listReply->disconnect(this);
        m_listReply->abort();
        m_listReply->deleteLater();
        m_listReply = nullptr;
    }

    m_listBusy = true;
    emit statusChanged(QStringLiteral("正在检查更新…"));

    QNetworkRequest request{QUrl(kReleasesApi)};
    request.setRawHeader("User-Agent", "Cjibackup-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(15000);

    const QString token = authToken();
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    m_listReply = m_net->get(request);
    connect(m_listReply, &QNetworkReply::finished, this,
            [this]() { handleListReply(m_listReply); });
}

void UpdateManager::handleListReply(QNetworkReply *reply)
{
    m_listBusy = false;
    if (!reply)
        return;

    m_listReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404)
            emit releasesFailed(QStringLiteral("GitHub 上还没有发布版本"));
        else
            emit releasesFailed(QStringLiteral("检查更新失败：%1").arg(reply->errorString()));
        return;
    }

    const QByteArray payload = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        emit releasesFailed(QStringLiteral("GitHub 返回的数据看不懂（可能触发限流）"));
        return;
    }

    QVector<UpdateRelease> releases;
    for (const QJsonValue &value : doc.array()) {
        const QJsonObject object = value.toObject();
        if (object.value("draft").toBool())
            continue;

        UpdateRelease release;
        release.version = normalizeVersion(object.value("tag_name").toString());
        if (release.version.isEmpty())
            continue;

        release.title = object.value("name").toString().trimmed();
        if (release.title.isEmpty())
            release.title = QStringLiteral("v%1").arg(release.version);
        release.date = object.value("published_at").toString().left(10);
        release.notes = parseReleaseNotes(object.value("body").toString());
        release.pageUrl = object.value("html_url").toString();
        release.prerelease = object.value("prerelease").toBool();

        for (const QJsonValue &assetValue : object.value("assets").toArray()) {
            const QJsonObject asset = assetValue.toObject();
            const QString name = asset.value("name").toString();
            if (!name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
                continue;
            release.downloadUrl = asset.value("browser_download_url").toString();
            if (name.contains(QStringLiteral("Setup")) || name.contains(QStringLiteral("安装"))
                || name.contains(QStringLiteral("Installer")))
                break;
        }

        releases.append(release);
    }

    std::sort(releases.begin(), releases.end(),
              [](const UpdateRelease &a, const UpdateRelease &b) {
                  return compareVersions(a.version, b.version) > 0;
              });

    emit releasesFetched(releases);
}

void UpdateManager::downloadAndInstall(const QString &url, const QString &version)
{
    if (url.isEmpty()) {
        emit failed(QStringLiteral("这个版本没有提供安装包下载地址"));
        return;
    }
    if (m_downloadReply) {
        emit statusChanged(QStringLiteral("已有下载任务在进行…"));
        return;
    }

    const QString dir = QDir::tempPath();
    m_downloadPath = QDir(dir).filePath(
        QString("Cjibackup-Setup-%1-%2.exe")
            .arg(version, QDateTime::currentDateTime().toString("yyyyMMddHHmmss")));
    m_downloadFile = new QFile(m_downloadPath, this);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        emit failed(QStringLiteral("无法写入临时文件：%1").arg(m_downloadPath));
        delete m_downloadFile;
        m_downloadFile = nullptr;
        return;
    }

    emit statusChanged(QStringLiteral("正在下载 v%1 …").arg(version));

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", "Cjibackup-Updater");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);

    const QString token = authToken();
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    m_downloadReply = m_net->get(request);

    connect(m_downloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit progress(received, total); });

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile && m_downloadReply)
            m_downloadFile->write(m_downloadReply->readAll());
    });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this, version]() {
        QNetworkReply *reply = m_downloadReply;
        m_downloadReply = nullptr;
        if (reply) {
            if (m_downloadFile && reply->bytesAvailable() > 0)
                m_downloadFile->write(reply->readAll());
            reply->deleteLater();
        }

        if (m_downloadFile) {
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }

        if (!reply || reply->error() != QNetworkReply::NoError) {
            emit failed(QStringLiteral("下载失败，请稍后到 GitHub 手动下载"));
            return;
        }

        emit statusChanged(QStringLiteral("下载完成，正在启动安装包…"));
        if (QProcess::startDetached(m_downloadPath, QStringList()))
            emit installerLaunched();
        else
            emit failed(QStringLiteral("安装包启动失败：%1").arg(m_downloadPath));
        Q_UNUSED(version)
    });
}
