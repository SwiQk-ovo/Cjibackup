#ifndef TOAST_H
#define TOAST_H

#include <QWidget>

#include <functional>

class SquircleCard;
class QLabel;
class QPushButton;
class QTimer;
class QPropertyAnimation;

/**
 * 应用内暗色浮层提示（右下角滑入卡片），替代系统托盘气泡。
 * 单例：同一时间只显示一条，新提示会替换旧提示。
 */
class Toast : public QWidget
{
    Q_OBJECT

public:
    enum Kind { Info, Success, Warning, Error, Reminder };

    static void showToast(const QString &title, const QString &text, Kind kind = Info,
                          int durationMs = 5000, const QString &actionText = QString(),
                          std::function<void()> onAction = {});
    static void dismissAll();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    explicit Toast();

    void applyContent(const QString &title, const QString &text, Kind kind,
                      const QString &actionText, std::function<void()> onAction);
    void present(int durationMs);
    void hideToast();
    void reposition();

    SquircleCard *m_card = nullptr;
    QLabel *m_icon = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_text = nullptr;
    QPushButton *m_action = nullptr;
    QTimer *m_hideTimer = nullptr;
    QPropertyAnimation *m_slide = nullptr;
    QPropertyAnimation *m_fade = nullptr;
    QPoint m_hiddenPos;
    std::function<void()> m_onAction;
    int m_duration = 5000;
};

#endif // TOAST_H
