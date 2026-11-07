#ifndef WINUTILS_H
#define WINUTILS_H

#include <QString>

class QWidget;

namespace WinUtils {

// "E:/DCIM/100CANON" -> "E:\"
QString driveRoot(const QString &path);
// "E:\" -> "E: 可移动磁盘"
QString driveDescription(const QString &path);

bool isRemovableDrive(const QString &path);

// 拉起 Windows 自带的格式化对话框（用户自己点开始），返回 0 表示格式化完成
// 返回 -99 表示该路径不是本地磁盘
int openFormatDialog(const QString &path, QWidget *owner);

bool revealInExplorer(const QString &path);

// Windows 10/11 深色标题栏
void applyDarkTitleBar(QWidget *window);

} // namespace WinUtils

#endif // WINUTILS_H
