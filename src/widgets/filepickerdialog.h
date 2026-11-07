#ifndef FILEPICKERDIALOG_H
#define FILEPICKERDIALOG_H

#include <QDialog>
#include <QStringList>

class QComboBox;
class QFileSystemModel;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTreeView;

/**
 * 自绘的文件 / 文件夹选择框（深色主题，跟主界面一致）。
 * 原生 QFileDialog 在 Windows 上要么是浅色，要么非原生版本布局老旧，这里统一接管。
 */
class FilePickerDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { PickDirectory, PickFile };

    FilePickerDialog(Mode mode, QWidget *parent, const QString &title,
                     const QString &startPath = QString());

    QString selectedPath() const;
    void setNameFilter(const QString &filterText);

    static QString pickDirectory(QWidget *parent, const QString &title,
                                 const QString &startPath = QString());
    static QString pickFile(QWidget *parent, const QString &title, const QString &filterText,
                            const QString &startPath = QString());

protected:
    void accept() override;

private:
    void buildUi(const QString &title);
    void setCurrentPath(const QString &path);
    void refreshPlaces();
    void onRowActivated(const QModelIndex &index);
    void updateButtons();
    void goUp();
    void pickAndAccept();

    Mode m_mode;
    QString m_currentPath;

    QFileSystemModel *m_model = nullptr;
    QTreeView *m_view = nullptr;
    QListWidget *m_places = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_filterCombo = nullptr;
    QLabel *m_pathLabel = nullptr;
    QLabel *m_emptyHint = nullptr;
    QPushButton *m_acceptButton = nullptr;
    QPushButton *m_upButton = nullptr;
};

#endif // FILEPICKERDIALOG_H
