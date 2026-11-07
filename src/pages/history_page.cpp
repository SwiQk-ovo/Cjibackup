#include "history_page.h"
#include "core/historymanager.h"
#include "widgets/darkdialog.h"
#include "widgets/squircle.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QStandardPaths>
#include <QTextStream>

HistoryPage::HistoryPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(15);

    QHBoxLayout *topBar = new QHBoxLayout();
    QLabel *title = new QLabel("备份历史", this);
    title->setObjectName("pageTitle");
    m_exportBtn = new QPushButton("导出 CSV", this);
    connect(m_exportBtn, &QPushButton::clicked, this, &HistoryPage::exportCsv);
    m_clearBtn = new QPushButton("清空记录", this);
    connect(m_clearBtn, &QPushButton::clicked, this, &HistoryPage::clearHistory);
    topBar->addWidget(title);
    topBar->addStretch();
    topBar->addWidget(m_exportBtn);
    topBar->addWidget(m_clearBtn);
    layout->addLayout(topBar);

    auto *listWrap = new QWidget(this);
    auto *listGrid = new QGridLayout(listWrap);
    listGrid->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(listWrap);
    m_list->setObjectName("historyList");
    listGrid->addWidget(m_list, 0, 0);

    m_empty = new QLabel("还没有备份记录\n备份完成后，文件数、大小、平均速度都会记在这里", listWrap);
    m_empty->setObjectName("emptyState");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    listGrid->addWidget(m_empty, 0, 0);

    layout->addWidget(listWrap, 1);

    connect(HistoryManager::instance(), &HistoryManager::recordsChanged, this, &HistoryPage::refreshList);
    refreshList();
}

void HistoryPage::refreshList()
{
    m_list->clear();
    auto records = HistoryManager::instance()->records();
    for (const HistoryRecord &r : records) {
        QString text = QString("%1  |  %2个文件  |  %3 MB  |  %4")
            .arg(r.startTime.toString("yyyy-MM-dd hh:mm"))
            .arg(r.fileCount)
            .arg(r.totalBytes / 1024 / 1024)
            .arg(r.formatted ? "已格式化" : "未格式化");
        m_list->addItem(text);
    }
    if (m_empty)
        m_empty->setVisible(records.isEmpty());
}

void HistoryPage::clearHistory()
{
    if (DarkDialog::question(this, "确认", "确定要清空所有备份记录吗？", "清空", "取消")) {
        HistoryManager::instance()->clearAll();
    }
}

void HistoryPage::exportCsv()
{
    const QVector<HistoryRecord> records = HistoryManager::instance()->records();
    if (records.isEmpty()) {
        DarkDialog::information(this, "导出 CSV", "现在还没有备份记录，先备份一次再来导出吧。");
        return;
    }

    QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QDir::homePath();

    const QString path = QDir(dir).filePath(
        QString("KOPAL历史_%1.csv")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        DarkDialog::warning(this, "导出失败", QString("写不了这个文件：\n%1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "\xEF\xBB\xBF"; // BOM：Excel 打开中文才不乱码
    out << "开始时间,结束时间,文件数,总大小(MB),平均速度(MB/s),是否格式化,源目录,目标目录\n";

    for (const HistoryRecord &r : records) {
        const QStringList cells{
            r.startTime.toString("yyyy-MM-dd HH:mm:ss"),
            r.endTime.toString("yyyy-MM-dd HH:mm:ss"),
            QString::number(r.fileCount),
            QString::number(double(r.totalBytes) / 1024.0 / 1024.0, 'f', 2),
            QString::number(r.avgSpeed, 'f', 2),
            r.formatted ? QStringLiteral("是") : QStringLiteral("否"),
            r.sourcePath,
            r.targetPath};

        QStringList escaped;
        for (QString cell : cells) {
            cell.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            if (cell.contains(QLatin1Char(',')) || cell.contains(QLatin1Char('"')))
                cell = QStringLiteral("\"%1\"").arg(cell);
            escaped << cell;
        }
        out << escaped.join(QLatin1Char(',')) << "\n";
    }
    file.close();

    DarkDialog::information(
        this, "导出完成",
        QString("已经把 %1 条记录导出到桌面：\n%2").arg(records.size()).arg(QDir::toNativeSeparators(path)),
        "知道了");
}
