#ifndef ALBUM_PAGE_H
#define ALBUM_PAGE_H

#include <QWidget>

class QAbstractItemModel;
class QHBoxLayout;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QScrollArea;
class QScrollBar;
class MediaWallView;
class AlbumDelegate;
class QModelIndex;
class QToolButton;

/**
 * 素材浏览：把备份目标文件夹当相册翻。
 * 三种视图（网格 / 列表 / 平铺）+ 搜索 + 排序 + 面包屑 + 分批懒加载。
 */
class AlbumPage : public QWidget
{
    Q_OBJECT

public:
    enum ViewMode { GridMode = 0, ListMode = 1, TileMode = 2 };

    explicit AlbumPage(QWidget *parent = nullptr);

    void setRootPath(const QString &path);
    QString rootPath() const { return m_rootPath; }

    // 跟随设置里的目标文件夹
    void followTargetFolder();

protected:
    void showEvent(QShowEvent *event) override;


signals:
    void statusMessage(const QString &text);

private slots:
    void onViewModeChanged(int mode);
    void refresh();
    void onSearchChanged(const QString &text);
    void onSortChanged(int index);
    void onActivated(const QModelIndex &index);
    void goUp();

private:
    QHBoxLayout *buildToolbar();
    void relayoutGrid();
    void applyFilters();
    void updateBreadcrumb();
    void updateEmptyState();
    void openPlayer(const QString &path);

    QString m_rootPath;
    int m_mode = GridMode;
    QString m_filter;

    QScrollBar *m_scrollBar = nullptr;
    MediaWallView *m_wall = nullptr;
    AlbumDelegate *m_delegate = nullptr;
    QAbstractItemModel *m_model = nullptr;
    QLineEdit *m_search = nullptr;
    QComboBox *m_sort = nullptr;
    QLabel *m_breadcrumb = nullptr;
    QLabel *m_hint = nullptr;
    QLabel *m_empty = nullptr;
    QToolButton *m_gridBtn = nullptr;
    QToolButton *m_listBtn = nullptr;
    QToolButton *m_tileBtn = nullptr;
};

#endif // ALBUM_PAGE_H
