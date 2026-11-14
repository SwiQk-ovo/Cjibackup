#ifndef EVENTS_PAGE_H
#define EVENTS_PAGE_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

class QLabel;

class EventsPage : public QWidget
{
    Q_OBJECT
public:
    explicit EventsPage(QWidget *parent = nullptr);

private slots:
    void addEvent();
    void removeEvent();

private:
    void updateEmptyState();
    void refreshFromSettings();

    QLineEdit *m_input = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_empty = nullptr;
    QLabel *m_meta = nullptr;
    QPushButton *m_delBtn = nullptr;
};

#endif // EVENTS_PAGE_H
