#ifndef APPICONS_H
#define APPICONS_H

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace AppIcons {

/** 列表里用的几种线性小图标（全部用 QPainter 画，不依赖图片资源） */
enum class Glyph {
    Camera,   // 一次备份
    Tag,      // 事件
    Card      // 存储卡
};

QIcon icon(Glyph glyph, int size = 18, const QColor &color = QColor(0x8A, 0x93, 0xA5));
QPixmap pixmap(Glyph glyph, int size, const QColor &color = QColor(0x8A, 0x93, 0xA5));

} // namespace AppIcons

#endif // APPICONS_H
