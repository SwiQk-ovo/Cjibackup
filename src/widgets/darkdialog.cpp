#include "widgets/squircle.h"
#include "darkdialog.h"

#include <QDesktopServices>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dwmapi.h>
#endif

namespace {

void applyDarkTitleBar(QWidget *window)
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
#else
    Q_UNUSED(window)
#endif
}

QString glyphFor(int kind)
{
    switch (kind) {
    case DarkDialog::Question: return QStringLiteral("?");
    case DarkDialog::Warning:  return QStringLiteral("!");
    case DarkDialog::Error:    return QStringLiteral("×");
    case DarkDialog::Success:  return QStringLiteral("✓");
    default:                   return QStringLiteral("i");
    }
}

QColor accentFor(int kind)
{
    switch (kind) {
    case DarkDialog::Warning: return QColor(0xF5, 0x9E, 0x0B);
    case DarkDialog::Error:   return QColor(0xF8, 0x71, 0x71);
    case DarkDialog::Success: return QColor(0x34, 0xD3, 0x99);
    default:                  return QColor(0x3B, 0x82, 0xF6);
    }
}

/** 圆形图标（自绘，避免 QMessageBox 那种系统蓝色问号） */
class DialogIcon : public QWidget
{
public:
    DialogIcon(int kind, QWidget *parent)
        : QWidget(parent)
        , m_kind(kind)
    {
        setFixedSize(48, 48);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal size = qMin(width(), height());
        const QRectF circle((width() - size) / 2.0 + 1, (height() - size) / 2.0 + 1,
                            size - 2, size - 2);

        QLinearGradient gradient(circle.topLeft(), circle.bottomRight());
        gradient.setColorAt(0.0, accentFor(m_kind).lighter(125));
        gradient.setColorAt(1.0, accentFor(m_kind));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(circle);

        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(int(size * 0.5));
        font.setFamily(QStringLiteral("Microsoft YaHei UI"));
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(circle, Qt::AlignCenter, glyphFor(m_kind));
    }

private:
    int m_kind;
};

} // namespace

DarkDialog::DarkDialog(QWidget *parent, Kind kind, const QString &title, const QString &text)
    : QDialog(parent)
    , m_kind(kind)
{
    setObjectName(QStringLiteral("darkDialog"));
    setWindowTitle(title);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowTitleHint, true);
    setWindowFlag(Qt::WindowCloseButtonHint, true);
    setModal(true);
    setMinimumWidth(460);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(18);

    auto *content = new QHBoxLayout();
    content->setSpacing(16);

    content->addWidget(new DialogIcon(kind, this), 0, Qt::AlignTop);

    m_textLabel = new QLabel(text, this);
    m_textLabel->setObjectName(QStringLiteral("dialogText"));
    m_textLabel->setTextFormat(Qt::PlainText);
    m_textLabel->setWordWrap(true);
    m_textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_textLabel->setMinimumWidth(340);
    content->addWidget(m_textLabel, 1);

    root->addLayout(content);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(10);
    buttonRow->addStretch();
    root->addLayout(buttonRow);

    // 按钮在上面的 addButtons 里塞到这一行（通过属性记录）
    buttonRow->setObjectName(QStringLiteral("dialogButtonRow"));
}

void DarkDialog::addButtons(const QString &okText, const QString &cancelText, bool hasCancel)
{
    auto *row = findChild<QHBoxLayout *>(QStringLiteral("dialogButtonRow"));
    if (!row)
        return;

    if (hasCancel) {
        m_cancelBtn = new QPushButton(cancelText, this);
        m_cancelBtn->setObjectName(QStringLiteral("dialogSecondary"));
        m_cancelBtn->setCursor(Qt::PointingHandCursor);
        m_cancelBtn->setAutoDefault(false);
        connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        row->addWidget(m_cancelBtn);
    }

    m_okBtn = new QPushButton(okText, this);
    m_okBtn->setObjectName(QStringLiteral("dialogPrimary"));
    m_okBtn->setCursor(Qt::PointingHandCursor);
    m_okBtn->setDefault(true);
    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(m_okBtn);
}

bool DarkDialog::question(QWidget *parent, const QString &title, const QString &text,
                          const QString &okText, const QString &cancelText)
{
    DarkDialog dialog(parent, Question, title, text);
    dialog.addButtons(okText, cancelText, true);
    return dialog.exec() == QDialog::Accepted;
}

void DarkDialog::information(QWidget *parent, const QString &title, const QString &text,
                             const QString &okText)
{
    DarkDialog dialog(parent, Info, title, text);
    dialog.addButtons(okText, QString(), false);
    dialog.exec();
}

void DarkDialog::warning(QWidget *parent, const QString &title, const QString &text)
{
    DarkDialog dialog(parent, Warning, title, text);
    dialog.addButtons(QStringLiteral("知道了"), QString(), false);
    dialog.exec();
}

void DarkDialog::critical(QWidget *parent, const QString &title, const QString &text)
{
    DarkDialog dialog(parent, Error, title, text);
    dialog.addButtons(QStringLiteral("知道了"), QString(), false);
    dialog.exec();
}


bool DarkDialog::questionWithLink(QWidget *parent, const QString &title, const QString &text,
                                  const QString &okText, const QString &cancelText,
                                  const QString &linkText, const QUrl &url)
{
    DarkDialog dialog(parent, Question, title, text);
    dialog.addButtons(okText, cancelText, true);

    auto *row = dialog.findChild<QHBoxLayout *>(QStringLiteral("dialogButtonRow"));
    if (row && !linkText.isEmpty()) {
        auto *link = new QPushButton(linkText, &dialog);
        link->setObjectName(QStringLiteral("dialogLink"));
        link->setCursor(Qt::PointingHandCursor);
        link->setAutoDefault(false);
        QObject::connect(link, &QPushButton::clicked, &dialog, [url]() {
            QDesktopServices::openUrl(url);
        });
        row->insertWidget(0, link);
    }

    return dialog.exec() == QDialog::Accepted;
}
void DarkDialog::informationWithLink(QWidget *parent, const QString &title, const QString &text,
                                     const QString &linkText, const QUrl &url)
{
    DarkDialog dialog(parent, Info, title, text);
    dialog.addButtons(QStringLiteral("知道了"), QString(), false);

    auto *row = dialog.findChild<QHBoxLayout *>(QStringLiteral("dialogButtonRow"));
    if (row && !linkText.isEmpty()) {
        auto *link = new QPushButton(linkText, &dialog);
        link->setObjectName(QStringLiteral("dialogLink"));
        link->setCursor(Qt::PointingHandCursor);
        link->setAutoDefault(false);
        QObject::connect(link, &QPushButton::clicked, &dialog, [url]() {
            QDesktopServices::openUrl(url);
        });
        row->insertWidget(0, link);
    }
    dialog.exec();
}
