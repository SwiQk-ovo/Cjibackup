#ifndef HISTORY_PAGE_H
#define HISTORY_PAGE_H

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

class HistoryPage : public QWidget
{
    Q_OBJECT
public:
    explicit HistoryPage(QWidget *parent = nullptr);

private slots:
    void refreshList();
    void clearHistory();
    void exportCsv();

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_exportBtn = nullptr;
    QLabel *m_empty = nullptr;
    QLabel *m_meta = nullptr;
};

#endif // HISTORY_PAGE_H
