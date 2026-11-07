#ifndef CHANGELOGDIALOG_H
#define CHANGELOGDIALOG_H

#include <QDialog>

class QWidget;

/**
 * 软件内「更新日志」窗口。
 *
 * 版式：标题 + Ver.x.y.z，下面是当前版本的分条说明（新增 / 修复 / 优化 / 调整 小胶囊），
 * 再往下是两块可折叠区域：「历史更新日志」和「TODO / 已知问题」。
 * 数据全部来自 src/core/changelog.cpp，不联网。
 */
class ChangelogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangelogDialog(QWidget *parent = nullptr);

    // 直接弹出（父窗口为空时挂到主窗口）
    static void showChangelog(QWidget *parent = nullptr);
};

#endif // CHANGELOGDIALOG_H
