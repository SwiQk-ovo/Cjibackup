#ifndef SEPARATORDIALOG_H
#define SEPARATORDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>

class QLabel;
class QLineEdit;
class QPushButton;

/**
 * 合并节点时选择分隔符（. - _ 空格 自定义）。
 * 选过的分隔符会记住，下次弹窗直接选中（"首次记住"）。
 */
class SeparatorDialog : public QDialog
{
    Q_OBJECT

public:
    // sampleParts 是合并节点里各项的示例文字（如 2026 / 10 / 31），只用来做预览
    SeparatorDialog(const QStringList &sampleParts, const QString &initial,
                    QWidget *parent = nullptr);

    QString separator() const { return m_separator; }

    static bool choose(QWidget *parent, const QString &initial, const QStringList &sampleParts,
                       QString *separator);

private:
    void selectOption(const QString &sep);
    void refreshPreview();

    QVector<QPushButton *> m_chips;
    QLineEdit *m_customEdit = nullptr;
    QLabel *m_preview = nullptr;
    QStringList m_sampleParts;
    QString m_separator = QStringLiteral("-");
};

#endif // SEPARATORDIALOG_H
