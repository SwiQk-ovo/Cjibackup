#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSet>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTextEdit>
#include <QTextStream>
#include <QUrlQuery>
#include <QVBoxLayout>

class Publisher : public QWidget
{
public:
    Publisher();

private:
    void pickFiles();
    void publish();
    void appendLog(const QString &text, const QString &color = QStringLiteral("#d5dae4"));
    QString readToken() const;
    QByteArray request(const QByteArray &verb, const QUrl &url, const QJsonObject *json,
                       const QString &uploadPath = QString());

    QLineEdit *m_owner = nullptr;
    QLineEdit *m_repo = nullptr;
    QLineEdit *m_version = nullptr;
    QLineEdit *m_title = nullptr;
    QLineEdit *m_notesFile = nullptr;
    QTextEdit *m_notes = nullptr;
    QListWidget *m_files = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_go = nullptr;
    QNetworkAccessManager *m_net = nullptr;
    QString m_token;
};

Publisher::Publisher()
{
    setWindowTitle(QStringLiteral("KOPAL · 发布工具"));
    resize(880, 760);
    m_net = new QNetworkAccessManager(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto *form = new QFormLayout();
    form->setSpacing(8);

    m_owner = new QLineEdit(QStringLiteral("SwiQk-ovo"));
    m_repo = new QLineEdit(QStringLiteral("Cjibackup"));
    auto *repoRow = new QHBoxLayout();
    repoRow->addWidget(m_owner);
    repoRow->addWidget(new QLabel(QStringLiteral("/")));
    repoRow->addWidget(m_repo);
    form->addRow(QStringLiteral("仓库"), repoRow);

    m_version = new QLineEdit();
    m_version->setPlaceholderText(QStringLiteral("1.0.8（不要带 v）"));
    form->addRow(QStringLiteral("版本号"), m_version);

    m_title = new QLineEdit();
    m_title->setPlaceholderText(QStringLiteral("留空自动用 KOPALv版本号"));
    form->addRow(QStringLiteral("标题"), m_title);

    auto *notesRow = new QHBoxLayout();
    m_notesFile = new QLineEdit();
    m_notesFile->setPlaceholderText(QStringLiteral("可留空，直接手写"));
    auto *browse = new QPushButton(QStringLiteral("选择说明文件"));
    notesRow->addWidget(m_notesFile);
    notesRow->addWidget(browse);
    form->addRow(QStringLiteral("说明文件"), notesRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("选择发布说明"), QString(),
                                                       QStringLiteral("文本 (*.txt *.md)"));
        if (f.isEmpty())
            return;
        m_notesFile->setText(f);
        QFile file(f);
        if (file.open(QIODevice::ReadOnly))
            m_notes->setPlainText(QString::fromUtf8(file.readAll()));
    });

    root->addLayout(form);

    m_notes = new QTextEdit();
    m_notes->setPlaceholderText(QStringLiteral("发布说明（纯文本，直接粘贴）"));
    m_notes->setMinimumHeight(150);
    root->addWidget(m_notes);

    auto *fileRow = new QHBoxLayout();
    auto *addFiles = new QPushButton(QStringLiteral("添加附件"));
    auto *delFile = new QPushButton(QStringLiteral("移除选中"));
    auto *autoFind = new QPushButton(QStringLiteral("从 CLesson 自动找"));
    fileRow->addWidget(addFiles);
    fileRow->addWidget(delFile);
    fileRow->addWidget(autoFind);
    fileRow->addStretch();
    root->addLayout(fileRow);

    m_files = new QListWidget();
    m_files->setMaximumHeight(110);
    root->addWidget(m_files);

    connect(addFiles, &QPushButton::clicked, this, &Publisher::pickFiles);
    connect(delFile, &QPushButton::clicked, this, [this]() { delete m_files->currentItem(); });
    connect(autoFind, &QPushButton::clicked, this, [this]() {
        const QString v = m_version->text().trimmed();
        if (v.isEmpty())
            return;
        const QString dir = QString("C:/Users/46051/Desktop/CLesson/版本/v%1/正式版").arg(v);
        if (!QDir(dir).exists()) {
            appendLog(QStringLiteral("没找到目录：") + dir, QStringLiteral("#f59e0b"));
            return;
        }
        m_files->clear();
        for (const QFileInfo &fi : QDir(dir).entryInfoList(QDir::Files))
            m_files->addItem(fi.absoluteFilePath());
        appendLog(QStringLiteral("已载入 %1 个附件").arg(m_files->count()), QStringLiteral("#34d399"));
    });

    m_go = new QPushButton(QStringLiteral("发布到 GitHub"));
    m_go->setMinimumHeight(38);
    root->addWidget(m_go);
    connect(m_go, &QPushButton::clicked, this, &Publisher::publish);

    m_bar = new QProgressBar();
    m_bar->setRange(0, 100);
    m_bar->setTextVisible(true);
    root->addWidget(m_bar);

    m_log = new QPlainTextEdit();
    m_log->setReadOnly(true);
    m_log->setMinimumHeight(180);
    root->addWidget(m_log, 1);

    // 记住上次填的内容
    QSettings st(QStringLiteral("Cjibackup"), QStringLiteral("PublishTool"));
    m_owner->setText(st.value(QStringLiteral("owner"), m_owner->text()).toString());
    m_repo->setText(st.value(QStringLiteral("repo"), m_repo->text()).toString());
    m_notes->setPlainText(st.value(QStringLiteral("notes")).toString());

    m_token = readToken();
    if (m_token.isEmpty())
        appendLog(QStringLiteral("没有找到 token：请把 token 存到程序同目录的 .token，或 F:\\Cjibackup\\release\\.token"),
                  QStringLiteral("#f87171"));
    else
        appendLog(QStringLiteral("已读取 token（%1…）").arg(m_token.left(7)), QStringLiteral("#34d399"));
}

QString Publisher::readToken() const
{
    QStringList paths{
        QCoreApplication::applicationDirPath() + QStringLiteral("/.token"),
        QStringLiteral("F:/Cjibackup/release/.token"),
        QDir::homePath() + QStringLiteral("/.cji_token")};
    for (const QString &p : paths) {
        QFile f(p);
        if (f.open(QIODevice::ReadOnly)) {
            const QString t = QString::fromUtf8(f.readAll()).trimmed();
            if (!t.isEmpty())
                return t;
        }
    }
    return qEnvironmentVariable("GH_TOKEN");
}

void Publisher::appendLog(const QString &text, const QString &color)
{
    m_log->appendHtml(QString("<span style='color:%1'>%2</span>")
                          .arg(color, text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
    QTextStream(stdout) << text << "\n";
}

void Publisher::pickFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("选择要上传的附件"));
    for (const QString &f : files)
        m_files->addItem(f);
}

QByteArray Publisher::request(const QByteArray &verb, const QUrl &url, const QJsonObject *json,
                              const QString &uploadPath)
{
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("token " + m_token).toUtf8());
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    if (json)
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    else if (!uploadPath.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));

    QNetworkReply *reply = nullptr;
    QFile *file = nullptr;
    if (!uploadPath.isEmpty()) {
        file = new QFile(uploadPath);
        if (!file->open(QIODevice::ReadOnly)) {
            delete file;
            return QByteArray();
        }
        reply = m_net->post(req, file);
        file->setParent(reply);
        connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 sent, qint64 total) {
            if (total > 0)
                m_bar->setValue(int(sent * 100 / total));
        });
    } else if (json) {
        reply = m_net->sendCustomRequest(req, verb, QJsonDocument(*json).toJson(QJsonDocument::Compact));
    } else {
        reply = m_net->sendCustomRequest(req, verb);
    }

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (reply->isFinished())
        loop.quit();
    else
        loop.exec();

    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

void Publisher::publish()
{
    if (m_token.isEmpty()) {
        m_token = readToken();
        if (m_token.isEmpty()) {
            appendLog(QStringLiteral("没有 token，先把 token 存到 .token 文件"), QStringLiteral("#f87171"));
            return;
        }
    }

    const QString owner = m_owner->text().trimmed();
    const QString repo = m_repo->text().trimmed();
    QString version = m_version->text().trimmed();
    if (version.startsWith(QLatin1Char('v')) || version.startsWith(QLatin1Char('V')))
        version = version.mid(1);
    if (version.isEmpty()) {
        appendLog(QStringLiteral("版本号不能为空"), QStringLiteral("#f87171"));
        return;
    }

    if (!m_notesFile->text().trimmed().isEmpty()) {
        QFile f(m_notesFile->text().trimmed());
        if (f.open(QIODevice::ReadOnly))
            m_notes->setPlainText(QString::fromUtf8(f.readAll()));
    }

    QString title = m_title->text().trimmed();
    if (title.isEmpty())
        title = QStringLiteral("KOPALv%1").arg(version);
    const QString body = m_notes->toPlainText();
    const QString tag = QStringLiteral("v%1").arg(version);

    QSettings st(QStringLiteral("Cjibackup"), QStringLiteral("PublishTool"));
    st.setValue(QStringLiteral("owner"), owner);
    st.setValue(QStringLiteral("repo"), repo);
    st.setValue(QStringLiteral("notes"), body);

    m_go->setEnabled(false);
    m_bar->setValue(0);

    const QUrl base(QStringLiteral("https://api.github.com/repos/%1/%2").arg(owner, repo));

    appendLog(QStringLiteral("================ 开始发布 %1 → %2 ================").arg(repo, tag));

    // 1. 查已有 release
    QJsonObject found;
    const QByteArray existing = request("GET", QUrl(base.toString() + "/releases/tags/" + tag), nullptr);
    if (!existing.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(existing);
        if (doc.isObject())
            found = doc.object();
    }

    QJsonObject release;
    if (!found.isEmpty()) {
        appendLog(QStringLiteral("该 tag 已有 release（id=%1），更新标题与说明").arg(found.value("id").toInt()),
                  QStringLiteral("#f59e0b"));
        QJsonObject patch;
        patch.insert("name", title);
        patch.insert("body", body);
        const QByteArray res = request("PATCH", QUrl(base.toString() + QStringLiteral("/releases/%1").arg(found.value("id").toInt())), &patch);
        release = QJsonDocument::fromJson(res).object();
    } else {
        QJsonObject create;
        create.insert("tag_name", tag);
        create.insert("target_commitish", "main");
        create.insert("name", title);
        create.insert("body", body);
        create.insert("draft", false);
        create.insert("prerelease", false);
        const QByteArray res = request("POST", QUrl(base.toString() + "/releases"), &create);
        release = QJsonDocument::fromJson(res).object();
    }

    const int releaseId = release.value("id").toInt();
    if (releaseId == 0) {
        appendLog(QStringLiteral("创建 / 更新 release 失败，检查 token 权限或网络"), QStringLiteral("#f87171"));
        m_go->setEnabled(true);
        return;
    }
    appendLog(QStringLiteral("release 就绪：") + release.value("html_url").toString(), QStringLiteral("#34d399"));

    // 2. 上传附件
    QSet<QString> have;
    for (const QJsonValue &v : release.value("assets").toArray())
        have.insert(v.toObject().value("name").toString());

    int uploaded = 0;
    for (int i = 0; i < m_files->count(); ++i) {
        const QString path = m_files->item(i)->text();
        const QFileInfo info(path);
        if (!info.exists()) {
            appendLog(QStringLiteral("附件不存在：") + path, QStringLiteral("#f87171"));
            continue;
        }
        if (have.contains(info.fileName())) {
            appendLog(QStringLiteral("跳过已存在附件：") + info.fileName(), QStringLiteral("#f59e0b"));
            continue;
        }

        appendLog(QStringLiteral("上传 %1（%2 MB）…").arg(info.fileName()).arg(info.size() / 1024.0 / 1024.0, 0, 'f', 1));
        const QUrl up(QStringLiteral("https://uploads.github.com/repos/%1/%2/releases/%3/assets?name=%4")
                          .arg(owner, repo, QString::number(releaseId), info.fileName()));
        const QByteArray res = request("POST", up, nullptr, path);
        const QJsonObject obj = QJsonDocument::fromJson(res).object();
        if (obj.contains("name")) {
            appendLog(QStringLiteral("上传完成：") + obj.value("name").toString(), QStringLiteral("#34d399"));
            ++uploaded;
        } else {
            appendLog(QStringLiteral("上传失败：") + info.fileName() + QStringLiteral(" → ") + QString::fromUtf8(res.left(200)),
                      QStringLiteral("#f87171"));
        }
    }

    m_bar->setValue(100);
    appendLog(QStringLiteral("完成，共上传 %1 个附件").arg(uploaded), QStringLiteral("#34d399"));
    appendLog(release.value("html_url").toString(), QStringLiteral("#60a5fa"));
    appendLog(QStringLiteral("提示：客户端「检查更新」现在可以检测到这个版本"), QStringLiteral("#8b95a7"));
    m_go->setEnabled(true);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CjiPublisher"));
    app.setStyleSheet(QStringLiteral(
        "QWidget{background:#12161f;color:#e6ebf3;font:13px 'Microsoft YaHei UI';}"
        "QLineEdit,QTextEdit,QPlainTextEdit,QListWidget{background:#0f131b;border:1px solid #242c3a;"
        "border-radius:8px;padding:6px;selection-background-color:#3b82f6;}"
        "QLabel{color:#8b95a7;}"
        "QPushButton{background:#3b82f6;color:#fff;border:0;border-radius:8px;padding:8px 16px;font-weight:600;}"
        "QPushButton:hover{background:#2f6fd0;}"
        "QPushButton:disabled{background:#2a3140;color:#6b7484;}"
        "QProgressBar{border:1px solid #242c3a;border-radius:6px;text-align:center;height:18px;}"
        "QProgressBar::chunk{background:#3b82f6;border-radius:5px;}"));

    Publisher w;
    w.show();
    return app.exec();
}
