#ifndef FILEDIALOGS_H
#define FILEDIALOGS_H

#include <QString>

class QWidget;

namespace UiDialogs {

// 非原生文件对话框：才能套用暗色主题，避免和主界面割裂
QString chooseDirectory(QWidget *parent, const QString &title, const QString &initial = QString());
QString chooseOpenFile(QWidget *parent, const QString &title, const QString &filter,
                       const QString &initial = QString());

} // namespace UiDialogs

#endif // FILEDIALOGS_H
