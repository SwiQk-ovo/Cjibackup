#include "widgets/squircle.h"
#include "events_page.h"
#include "core/settingsmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QGridLayout>
#include <QLabel>
#include "widgets/appicons.h"

EventsPage::EventsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(15);

    auto *topBar = new QHBoxLayout();
    QLabel *title = new QLabel("事件管理", this);
    title->setObjectName("pageTitle");
    topBar->addWidget(title);

    m_meta = new QLabel(this);
    m_meta->setObjectName("pageMeta");
    topBar->addSpacing(12);
    topBar->addWidget(m_meta);
    topBar->addStretch();
    layout->addLayout(topBar);

    QLabel *hint = new QLabel("事件会作为流程图里「事件」分类的候选名称，例如 校运会 / 婚礼。", this);
    hint->setObjectName("hintLabel");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QHBoxLayout *addRow = new QHBoxLayout();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("输入事件名称，如：校运会、婚礼...");
    QPushButton *addBtn = new QPushButton("添加", this);
    connect(addBtn, &QPushButton::clicked, this, &EventsPage::addEvent);
    connect(m_input, &QLineEdit::returnPressed, this, &EventsPage::addEvent);
    addRow->addWidget(m_input);
    addRow->addWidget(addBtn);
    layout->addLayout(addRow);

    // 列表 + 空状态叠在一起，没内容时显示提示而不是一片空白
    auto *listWrap = new QWidget(this);
    auto *listGrid = new QGridLayout(listWrap);
    listGrid->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(listWrap);
    m_list->setObjectName("eventsList");
    m_list->setIconSize(QSize(18, 18));
    for (const QString &name : SettingsManager::instance()->events()) {
        auto *item = new QListWidgetItem(AppIcons::icon(AppIcons::Glyph::Tag, 18), name, m_list);
        item->setSizeHint(QSize(0, 38));
    }
    listGrid->addWidget(m_list, 0, 0);

    m_empty = new QLabel("还没有事件\n把常拍的类型（婚礼、校运会……）加进来，画流程图选「事件」时就能直接挑",
                         listWrap);
    m_empty->setObjectName("emptyState");
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    listGrid->addWidget(m_empty, 0, 0);

    layout->addWidget(listWrap, 1);

    auto *delRow = new QHBoxLayout();
    delRow->addStretch(1);
    m_delBtn = new QPushButton("删除选中", this);
    m_delBtn->setObjectName("ghostBtn");
    m_delBtn->setCursor(Qt::PointingHandCursor);
    m_delBtn->setEnabled(false);
    connect(m_delBtn, &QPushButton::clicked, this, &EventsPage::removeEvent);
    delRow->addWidget(m_delBtn);
    layout->addLayout(delRow);

    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        m_delBtn->setEnabled(m_list->currentItem() != nullptr);
    });

    updateEmptyState();
}

void EventsPage::updateEmptyState()
{
    if (m_empty)
        m_empty->setVisible(m_list->count() == 0);
    if (m_meta)
        m_meta->setText(m_list->count() == 0 ? QStringLiteral("暂无事件")
                                             : QStringLiteral("共 %1 个").arg(m_list->count()));
    if (m_delBtn)
        m_delBtn->setEnabled(m_list->currentItem() != nullptr);
}

void EventsPage::addEvent()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;

    SettingsManager *sm = SettingsManager::instance();
    sm->addEvent(text);

    if (m_list->findItems(text, Qt::MatchExactly).isEmpty()) {
        auto *item = new QListWidgetItem(AppIcons::icon(AppIcons::Glyph::Tag, 18), text, m_list);
        item->setSizeHint(QSize(0, 38));
    }

    m_input->clear();
    m_list->setCurrentRow(m_list->count() - 1);
}

void EventsPage::removeEvent()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item)
        return;

    SettingsManager::instance()->removeEvent(item->text());
    delete m_list->takeItem(m_list->row(item));
}
