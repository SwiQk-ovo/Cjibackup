#ifndef ABOUT_PAGE_H
#define ABOUT_PAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class QPushButton;

class AboutPage : public QWidget
{
    Q_OBJECT
public:
    explicit AboutPage(QWidget *parent = nullptr);

    QWidget *guideButtonWidget() const { return m_guideBtn; }

signals:
    // 点「使用引导」：重新走一遍首次启动的引导
    void guideRequested();

private:
    QPushButton *m_guideBtn = nullptr;
};

#endif // ABOUT_PAGE_H
