#ifndef TYPEWRITERTOOLTIP_H
#define TYPEWRITERTOOLTIP_H

#include <QLabel>
#include <QPointer>
#include <QString>
#include <QWidget>

class QTimer;
class QEnterEvent;

/**
 * 打字机提示气泡：
 * - 悬停时在窗口上方居中显示，文字逐字打出
 * - 鼠标离开时逐字删除后消失
 */
class TypewriterTooltip : public QWidget
{
    Q_OBJECT

public:
    static TypewriterTooltip *instance();

    void showText(const QString &text, QWidget *anchor);
    QString visibleText() const { return m_text.left(m_visibleChars); }
    void dismiss();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void tick();

private:
    explicit TypewriterTooltip(QWidget *parent = nullptr);
    void applyGeometry();

    QString m_text;
    int m_visibleChars = 0;
    bool m_deleting = false;
    int m_idleTicks = 0;
    QTimer *m_timer = nullptr;
    QPointer<QWidget> m_anchor;
};

/** 设置项后面的小感叹号，悬停触发打字机提示 */
class HelpIcon : public QLabel
{
    Q_OBJECT

public:
    explicit HelpIcon(const QString &text, QWidget *parent = nullptr);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_text;
};

#endif // TYPEWRITERTOOLTIP_H
