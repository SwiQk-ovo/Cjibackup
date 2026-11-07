#ifndef APPICON_H
#define APPICON_H

#include <QIcon>
#include <QPixmap>

namespace AppIcon {

// 程序运行时图标（窗口 / 托盘），全部由代码绘制，不依赖图片资源
QIcon applicationIcon();
QPixmap logoPixmap(int size);

} // namespace AppIcon

#endif // APPICON_H
