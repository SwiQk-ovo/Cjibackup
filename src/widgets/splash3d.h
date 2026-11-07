#ifndef SPLASH3D_H
#define SPLASH3D_H

#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QRectF>

namespace Splash3d {

// 把矩形按 rotX / rotY（角度）做透视投影，返回四个角
QPolygonF projectQuad(const QRectF &rect, qreal rotX, qreal rotY, qreal scale = 1.0,
                      qreal fov = 620.0);

// 按 3D 姿态绘制整张图；返回是否绘制成功（侧面接近 90° 时会失败）
bool drawPixmap3D(QPainter &painter, const QPixmap &pixmap, const QRectF &rect,
                  qreal rotX, qreal rotY, qreal scale = 1.0, qreal opacity = 1.0,
                  qreal fov = 620.0);

// 只画源图里的一块（立体切片用）
bool drawPixmapSlice3D(QPainter &painter, const QPixmap &pixmap, const QRectF &source,
                       const QRectF &target, qreal rotX, qreal rotY, qreal scale = 1.0,
                       qreal opacity = 1.0, qreal fov = 620.0);

// 地面倒影（带向下渐隐）
void drawReflection(QPainter &painter, const QPixmap &pixmap, const QRectF &rect,
                    qreal opacity, const QColor &fadeColor, qreal gap = 8.0,
                    qreal heightFactor = 0.45);

// 接触阴影
void drawContactShadow(QPainter &painter, const QPointF &center, qreal radiusX, qreal radiusY,
                       qreal alpha);

// 斜向高光扫过（贴在投影四边形里）
void drawSpecular(QPainter &painter, const QPolygonF &quad, qreal phase,
                  const QColor &tint = QColor(180, 215, 255));

} // namespace Splash3d

#endif // SPLASH3D_H
