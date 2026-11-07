#include "widgets/squircle.h"
#include "filepickerdialog.h"

#include "core/winutils.h"

#include <QComboBox>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStorageInfo>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QItemSelectionModel>
#include <QStyle>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <windows.h>

namespace {

// 深色标题栏（动态取 dwmapi，别把 winutils 依赖带进安装器目标）
void applyDarkCaption(QWidget *widget)
{
    if (!widget)
        return;

    using SetAttribute = HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    static SetAttribute setAttribute = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll"))
            setAttribute = reinterpret_cast<SetAttribute>(
                reinterpret_cast<void *>(GetProcAddress(dwm, "DwmSetWindowAttribute")));
    }
    if (!setAttribute)
        return;

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    BOOL dark = TRUE;
    setAttribute(hwnd, 20, &dark, sizeof(dark)); // DWMWA_USE_IMMERSIVE_DARK_MODE
    setAttribute(hwnd, 19, &dark, sizeof(dark)); // 旧版
}

QString prettyPath(const QString &path)
{
    QString text = QDir::toNativeSeparators(path);
    if (text.startsWith(QStringLiteral("\\\\?\\")))
        text = text.mid(4);
    return text;
}

} // namespace

FilePickerDialog::FilePickerDialog(Mode mode, QWidget *parent, const QString &title,
                                   const QString &startPath)
    : QDialog(parent)
    , m_mode(mode)
{
    setObjectName(QStringLiteral("filePicker"));
    if (parent)
        setWindowIcon(parent->windowIcon());
    setWindowTitle(title);
    setMinimumSize(860, 560);
    resize(920, 600);

    QString start = startPath;
    if (start.isEmpty() || !QDir(start).exists())
        start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (start.isEmpty() || !QDir(start).exists())
        start = QDir::homePath();

    buildUi(title);
    setCurrentPath(start);

    // 原生窗口建好之后再应用深色标题栏
    QTimer::singleShot(0, this, [this]() { applyDarkCaption(this); });
}

void FilePickerDialog::buildUi(const QString &title)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    // 顶部：返回 / 上一级 / 路径
    auto *pathRow = new QHBoxLayout();
    pathRow->setSpacing(8);

    m_upButton = new QPushButton(QStringLiteral("上一级"), this);
    m_upButton->setObjectName(QStringLiteral("ghostBtn"));
    m_upButton->setCursor(Qt::PointingHandCursor);
    m_upButton->setToolTip(QStringLiteral("回到上一级文件夹"));
    connect(m_upButton, &QPushButton::clicked, this, &FilePickerDialog::goUp);

    auto *newFolderBtn = new QPushButton(QStringLiteral("新建文件夹"), this);
    newFolderBtn->setObjectName(QStringLiteral("ghostBtn"));
    newFolderBtn->setCursor(Qt::PointingHandCursor);
    newFolderBtn->setToolTip(QStringLiteral("在当前文件夹里新建一个文件夹"));
    connect(newFolderBtn, &QPushButton::clicked, this, [this]() {
        QDir dir(m_currentPath);
        if (!dir.exists())
            return;

        // 直接建「新建文件夹」，重名就加序号，建完在列表里选中
        QString name = QStringLiteral("新建文件夹");
        int index = 2;
        while (dir.exists(name))
            name = QStringLiteral("新建文件夹 (%1)").arg(index++);

        if (!dir.mkdir(name))
            return;

        const QString created = dir.filePath(name);
        setCurrentPath(m_currentPath);
        const QModelIndex index2 = m_model->index(created);
        if (index2.isValid()) {
            m_view->setCurrentIndex(index2);
            m_view->scrollTo(index2);
            if (m_nameEdit)
                m_nameEdit->setText(name);
        }
    });

    m_pathLabel = new QLabel(this);
    m_pathLabel->setObjectName(QStringLiteral("pickerPath"));
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    pathRow->addWidget(m_upButton);
    pathRow->addWidget(newFolderBtn);
    pathRow->addWidget(m_pathLabel, 1);
    root->addLayout(pathRow);

    // 主体：左侧常用位置 + 右侧文件列表
    auto *body = new QHBoxLayout();
    body->setSpacing(12);

    m_places = new QListWidget(this);
    m_places->setObjectName(QStringLiteral("pickerPlaces"));
    m_places->setFixedWidth(168);
    m_places->setFrameShape(QFrame::NoFrame);
    m_places->setIconSize(QSize(16, 16));
    connect(m_places, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item)
            return;
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            setCurrentPath(path);
    });
    body->addWidget(m_places);

    auto *listBox = new QVBoxLayout();
    listBox->setSpacing(0);

    m_model = new QFileSystemModel(this);
    m_model->setRootPath(QDir::rootPath());
    if (m_mode == Mode::PickDirectory)
        m_model->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Drives);
    else
        m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Drives);

    m_view = new QTreeView(this);
    m_view->setObjectName(QStringLiteral("pickerView"));
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setItemsExpandable(false);
    m_view->setIndentation(0);
    m_view->setUniformRowHeights(true);
    m_view->setIconSize(QSize(18, 18));
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_view->setColumnHidden(1, true);   // 大小
    m_view->setColumnHidden(2, true);   // 类型（系统给的是英文 File folder，很难看）
    m_view->setColumnHidden(3, false);  // 修改日期
    m_view->header()->setSortIndicatorShown(false);
    m_view->header()->setHighlightSections(false);
    m_view->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_view->header()->resizeSection(3, 150);

    connect(m_view, &QTreeView::activated, this, &FilePickerDialog::onRowActivated);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]() { updateButtons(); });

    m_emptyHint = new QLabel(QStringLiteral("这个文件夹是空的"), this);
    m_emptyHint->setObjectName(QStringLiteral("pickerHint"));
    m_emptyHint->hide();

    listBox->addWidget(m_view, 1);
    listBox->addWidget(m_emptyHint);
    body->addLayout(listBox, 1);
    root->addLayout(body, 1);

    // 底部：文件名 + 类型 + 按钮
    auto *bottom = new QHBoxLayout();
    bottom->setSpacing(10);

    auto *nameLabel = new QLabel(QStringLiteral("名称"), this);
    nameLabel->setObjectName(QStringLiteral("pickerFieldLabel"));

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setObjectName(QStringLiteral("pickerName"));
    m_nameEdit->setMinimumWidth(320);
    connect(m_nameEdit, &QLineEdit::returnPressed, this, [this]() { pickAndAccept(); });

    QLabel *filterLabel = nullptr;
    if (m_mode == Mode::PickFile) {
        filterLabel = new QLabel(QStringLiteral("类型"), this);
        filterLabel->setObjectName(QStringLiteral("pickerFieldLabel"));

        m_filterCombo = new QComboBox(this);
        m_filterCombo->setObjectName(QStringLiteral("pickerFilter"));
        m_filterCombo->setMinimumWidth(240);
    }

    m_acceptButton = new QPushButton(this);
    m_acceptButton->setObjectName(QStringLiteral("dialogPrimary"));
    m_acceptButton->setDefault(true);

    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    cancelButton->setObjectName(QStringLiteral("dialogSecondary"));
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_acceptButton, &QPushButton::clicked, this, [this]() { pickAndAccept(); });

    if (m_mode == Mode::PickFile) {
        bottom->addWidget(nameLabel);
        bottom->addWidget(m_nameEdit, 1);
        bottom->addSpacing(6);
        bottom->addWidget(filterLabel);
        bottom->addWidget(m_filterCombo);
        m_acceptButton->setText(QStringLiteral("打开"));
    } else {
        bottom->addWidget(nameLabel);
        bottom->addWidget(m_nameEdit, 1);
        m_acceptButton->setText(QStringLiteral("选择文件夹"));
        m_nameEdit->setReadOnly(true);
        m_nameEdit->setPlaceholderText(QStringLiteral("当前文件夹"));
    }
    bottom->addSpacing(8);
    bottom->addWidget(cancelButton);
    bottom->addWidget(m_acceptButton);
    root->addLayout(bottom);

    Q_UNUSED(title)
    refreshPlaces();
    updateButtons();
}

void FilePickerDialog::refreshPlaces()
{
    m_places->clear();

    const QFileIconProvider provider;
    const QIcon folderIcon = provider.icon(QFileIconProvider::Folder);

    struct Place
    {
        QString label;
        QString path;
    };
    const QVector<Place> places{
        {QStringLiteral("桌面"), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)},
        {QStringLiteral("下载"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)},
        {QStringLiteral("文档"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)},
        {QStringLiteral("图片"), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)},
        {QStringLiteral("视频"), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)},
    };

    for (const Place &place : places) {
        if (place.path.isEmpty() || !QDir(place.path).exists())
            continue;
        auto *item = new QListWidgetItem(folderIcon, place.label, m_places);
        item->setData(Qt::UserRole, place.path);
    }

    auto *sep = new QListWidgetItem(QStringLiteral("此电脑"), m_places);
    sep->setFlags(Qt::NoItemFlags);
    sep->setData(Qt::UserRole, QString());

    for (const QFileInfo &drive : QDir::drives()) {
        QString driveName = drive.absoluteFilePath();
        driveName.remove(QLatin1Char('/'));
        if (driveName.endsWith(QLatin1Char('\\')))
            driveName.chop(1);

        // 带上卷标，用户才分得清哪个盘是哪个
        QStorageInfo storage(drive.absoluteFilePath());
        QString label = storage.displayName().trimmed();
        // 没卷标的盘，displayName 会返回 "D:/" 这种，统一显示成「本地磁盘」
        const QString trimmedRoot = storage.rootPath().chopped(1);
        if (label.isEmpty() || label.startsWith(driveName, Qt::CaseInsensitive)
            || label.compare(trimmedRoot, Qt::CaseInsensitive) == 0)
            label = QStringLiteral("本地磁盘");

        const QString text = label.isEmpty() ? driveName
                                             : QStringLiteral("%1 (%2)").arg(label, driveName);
        auto *item = new QListWidgetItem(m_places->style()->standardIcon(QStyle::SP_DriveHDIcon),
                                         text, m_places);
        item->setData(Qt::UserRole, drive.absoluteFilePath());
    }
}

void FilePickerDialog::setCurrentPath(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists())
        return;

    QString target = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    target = QDir::toNativeSeparators(target);

    m_currentPath = target;
    m_pathLabel->setText(prettyPath(target));
    m_pathLabel->setToolTip(target);

    const QModelIndex index = m_model->index(target);
    m_view->setRootIndex(index);

    if (m_mode == Mode::PickDirectory)
        m_nameEdit->setText(target);
    else if (!info.isDir())
        m_nameEdit->setText(info.fileName());

    updateButtons();
}

void FilePickerDialog::goUp()
{
    QDir dir(m_currentPath);
    if (dir.cdUp())
        setCurrentPath(dir.absolutePath());
}

void FilePickerDialog::onRowActivated(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    const QFileInfo info = m_model->fileInfo(index);
    if (info.isDir()) {
        setCurrentPath(info.absoluteFilePath());
        return;
    }

    if (m_mode == Mode::PickFile)
        pickAndAccept();
}

void FilePickerDialog::updateButtons()
{
    if (!m_acceptButton)
        return;

    if (m_mode == Mode::PickDirectory) {
        const QModelIndex current = m_view->currentIndex();
        const bool hasFolder = current.isValid() && m_model->fileInfo(current).isDir();
        if (hasFolder)
            m_nameEdit->setText(m_model->fileInfo(current).absoluteFilePath());
        else
            m_nameEdit->setText(m_currentPath);
        m_acceptButton->setEnabled(true);
        return;
    }

    const QModelIndex current = m_view->currentIndex();
    const bool hasFile = current.isValid() && m_model->fileInfo(current).isFile();
    if (hasFile)
        m_nameEdit->setText(m_model->fileInfo(current).fileName());
    m_acceptButton->setEnabled(hasFile || !m_nameEdit->text().trimmed().isEmpty());
}

void FilePickerDialog::setNameFilter(const QString &filterText)
{
    if (!m_filterCombo)
        return;

    QStringList patterns;
    for (const QString &part : filterText.split(QStringLiteral(";;"))) {
        const int open = part.indexOf(QLatin1Char('('));
        const int close = part.lastIndexOf(QLatin1Char(')'));
        if (open < 0 || close <= open)
            continue;
        const QString inner = part.mid(open + 1, close - open - 1);
        for (const QString &pattern : inner.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            patterns << pattern.trimmed();
    }

    if (!patterns.isEmpty()) {
        m_model->setNameFilters(patterns);
        m_model->setNameFilterDisables(false);
    }

    m_filterCombo->addItem(filterText);
    m_filterCombo->setEnabled(false);
}

void FilePickerDialog::pickAndAccept()
{
    const QString text = m_nameEdit->text().trimmed();

    if (m_mode == Mode::PickDirectory) {
        const QString path = text.isEmpty() ? m_currentPath : text;
        if (QDir(path).exists())
            accept();
        return;
    }

    QString path = text;
    if (!QFileInfo(path).isAbsolute())
        path = QDir(m_currentPath).filePath(text);
    if (QFileInfo(path).isFile())
        accept();
}

void FilePickerDialog::accept()
{
    if (m_mode == Mode::PickFile && !QFileInfo(selectedPath()).isFile())
        return;
    QDialog::accept();
}

QString FilePickerDialog::selectedPath() const
{
    if (!m_nameEdit)
        return QString();

    const QString text = m_nameEdit->text().trimmed();
    if (text.isEmpty())
        return m_mode == Mode::PickDirectory ? m_currentPath : QString();
    if (QFileInfo(text).isAbsolute())
        return QDir::fromNativeSeparators(text);
    return QDir(m_currentPath).filePath(text);
}

QString FilePickerDialog::pickDirectory(QWidget *parent, const QString &title,
                                        const QString &startPath)
{
    FilePickerDialog dialog(PickDirectory, parent, title, startPath);
    if (dialog.exec() != QDialog::Accepted)
        return QString();
    return dialog.selectedPath();
}

QString FilePickerDialog::pickFile(QWidget *parent, const QString &title,
                                   const QString &filterText, const QString &startPath)
{
    FilePickerDialog dialog(PickFile, parent, title, startPath);
    dialog.setNameFilter(filterText);
    if (dialog.exec() != QDialog::Accepted)
        return QString();
    return dialog.selectedPath();
}
