#include "widgets/separatordialog.h"

#include "core/settingsmanager.h"
#include "core/winutils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// 文件夹名里不能出现的字符（允许空格）
QString cleanSeparator(const QString &raw)
{
    QString value = raw;
    const QString invalid = QStringLiteral("\\/:*?\"<>|");
    for (const QChar &c : invalid)
        value.remove(c);

    QString cleaned;
    cleaned.reserve(value.size());
    for (const QChar &c : value) {
        if (c.unicode() < 0x20)
            continue;
        cleaned.append(c);
    }
    return cleaned.left(4);
}

bool isPreset(const QString &sep)
{
    return sep == QStringLiteral(".") || sep == QStringLiteral("-")
           || sep == QStringLiteral("_") || sep == QStringLiteral(" ");
}

} // namespace

SeparatorDialog::SeparatorDialog(const QStringList &sampleParts, const QString &initial,
                                 QWidget *parent)
    : QDialog(parent)
    , m_sampleParts(sampleParts)
{
    setObjectName(QStringLiteral("separatorDialog"));
    setWindowTitle(QStringLiteral("合并节点"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setModal(true);
    setMinimumWidth(520);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("合并成一个文件夹"), this);
    title->setObjectName(QStringLiteral("sepTitle"));
    root->addWidget(title);

    auto *hint = new QLabel(
        QStringLiteral("合并后的节点只生成一层文件夹，中间用你选的分隔符连接。\n"
                       "之后双击这个合并节点，还能再改分隔符。"),
        this);
    hint->setObjectName(QStringLiteral("sepHint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    // 分隔符选项：. - _ 空格 自定义
    auto *chips = new QHBoxLayout();
    chips->setSpacing(8);

    const QVector<QPair<QString, QString>> options = {
        {QStringLiteral(". 点"), QStringLiteral(".")},
        {QStringLiteral("- 连字符"), QStringLiteral("-")},
        {QStringLiteral("_ 下划线"), QStringLiteral("_")},
        {QStringLiteral("空格"), QStringLiteral(" ")},
        {QStringLiteral("自定义"), QString()},
    };

    for (const auto &option : options) {
        auto *chip = new QPushButton(option.first, this);
        chip->setObjectName(QStringLiteral("sepChip"));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setCheckable(true);
        chip->setAutoDefault(false);
        chip->setProperty("sep", option.second);
        m_chips << chip;
        chips->addWidget(chip);
        connect(chip, &QPushButton::clicked, this, [this, chip]() {
            selectOption(chip->property("sep").toString());
        });
    }
    chips->addStretch(1);
    root->addLayout(chips);

    m_customEdit = new QLineEdit(this);
    m_customEdit->setPlaceholderText(QStringLiteral("自定义分隔符（最多 4 个字符）"));
    m_customEdit->setMaxLength(4);
    m_customEdit->setClearButtonEnabled(true);
    connect(m_customEdit, &QLineEdit::textEdited, this, [this]() { selectOption(QString()); });
    root->addWidget(m_customEdit);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("sepPreview"));
    m_preview->setWordWrap(true);
    m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_preview);

    auto *buttons = new QHBoxLayout();
    buttons->setSpacing(10);
    buttons->addStretch(1);

    auto *cancel = new QPushButton(QStringLiteral("取消"), this);
    cancel->setObjectName(QStringLiteral("dialogSecondary"));
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setAutoDefault(false);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);

    auto *ok = new QPushButton(QStringLiteral("确定"), this);
    ok->setObjectName(QStringLiteral("dialogPrimary"));
    ok->setCursor(Qt::PointingHandCursor);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, [this]() {
        // 以当前界面状态为准：自定义框里有字就用它，否则用选中的胶囊
        const QString custom = cleanSeparator(m_customEdit->text());
        if (!custom.isEmpty())
            m_separator = custom;
        accept();
    });
    buttons->addWidget(ok);

    root->addLayout(buttons);

    // 初始选中：上次记住的分隔符
    const QString remembered = cleanSeparator(initial);
    selectOption(remembered.isEmpty() ? QStringLiteral("-") : remembered);

    WinUtils::applyDarkTitleBar(this);
}

void SeparatorDialog::selectOption(const QString &sep)
{
    // sep 为空 = 用户正在自定义框里打字，直接读输入框
    const bool typing = sep.isEmpty() && m_customEdit->hasFocus();
    const QString value = typing ? cleanSeparator(m_customEdit->text()) : cleanSeparator(sep);
    const bool custom = typing || value.isEmpty() || !isPreset(value);

    for (QPushButton *chip : m_chips) {
        const QString chipValue = chip->property("sep").toString();
        const bool checked = chipValue.isEmpty() ? custom : (!custom && chipValue == value);
        chip->setChecked(checked);
    }

    if (custom) {
        if (!typing && !value.isEmpty())
            m_customEdit->setText(value);
        m_separator = cleanSeparator(m_customEdit->text());
        if (m_separator.isEmpty())
            m_separator = QStringLiteral("-");
    } else {
        m_separator = value;
        m_customEdit->clear();
    }

    refreshPreview();
}

void SeparatorDialog::refreshPreview()
{
    const QString sep = m_separator.isEmpty() ? QStringLiteral("-") : m_separator;
    QStringList parts = m_sampleParts;
    if (parts.isEmpty())
        parts << QStringLiteral("文件夹名");
    parts.removeAll(QString());

    m_preview->setText(QStringLiteral("预览：%1").arg(parts.join(sep)));
}

bool SeparatorDialog::choose(QWidget *parent, const QString &initial,
                             const QStringList &sampleParts, QString *separator)
{
    if (!separator)
        return false;

    SeparatorDialog dialog(sampleParts, initial, parent);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    const QString picked = dialog.separator().isEmpty() ? QStringLiteral("-")
                                                         : dialog.separator();
    *separator = picked;
    SettingsManager::instance()->setMergeSeparator(picked);
    return true;
}
