#ifndef DARKDIALOG_H
#define DARKDIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;
class QUrl;

/**
 * 和主界面同一套配色的消息框，替代 QMessageBox：
 * - 深色卡片 + 蓝色图标 + 中文按钮
 * - 深色标题栏（Windows 10/11）
 * - 纯文本显示，不会被更新说明里的富文本影响
 */
class DarkDialog : public QDialog
{
    Q_OBJECT

public:
    enum Kind { Info, Question, Warning, Error, Success };

    static bool question(QWidget *parent, const QString &title, const QString &text,
                         const QString &okText = QStringLiteral("确定"),
                         const QString &cancelText = QStringLiteral("取消"));

    static void information(QWidget *parent, const QString &title, const QString &text,
                            const QString &okText = QStringLiteral("知道了"));

    static void warning(QWidget *parent, const QString &title, const QString &text);
    static void critical(QWidget *parent, const QString &title, const QString &text);

    /**
     * 危险操作二次确认：第一次是普通问询；第二次确认按钮换到左边、窗口整体错开，
     * 避免手快连点两下就把危险操作点过去。
     */
    static bool confirmTwice(QWidget *parent, const QString &title, const QString &text,
                             const QString &okText = QStringLiteral("继续"));

    // 带「在浏览器中查看」按钮的提示
    static void informationWithLink(QWidget *parent, const QString &title, const QString &text,
                                    const QString &linkText, const QUrl &url);

    static bool questionWithLink(QWidget *parent, const QString &title, const QString &text,
                                 const QString &okText, const QString &cancelText,
                                 const QString &linkText, const QUrl &url);

private:
    explicit DarkDialog(QWidget *parent, Kind kind, const QString &title, const QString &text);
    void addButtons(const QString &okText, const QString &cancelText, bool hasCancel,
                    bool shiftPrimary = false);

    Kind m_kind = Info;
    QLabel *m_textLabel = nullptr;
    QPushButton *m_okBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

#endif // DARKDIALOG_H
