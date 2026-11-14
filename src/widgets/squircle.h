#ifndef SQUIRCLE_H
#define SQUIRCLE_H

#include <QColor>
#include <QFrame>
#include <QPushButton>
#include <QGroupBox>
#include <QPainterPath>
#include <QString>

namespace Squircle {

/**
 * 连续曲率圆角（超椭圆 / squircle）路径。
 *
 * Qt 的 border-radius 画的是正圆弧，圆弧和直边相接的地方曲率突然变化，
 * 看起来就是"圆角矩形"。这里把每个角画成 |x|^n + |y|^n = 1 的超椭圆弧，
 * 切点处曲率更小、过渡更顺，视觉上就是 iOS / Figma 那种连续曲率圆角。
 *
 * exponent 越大越"方"，4.0 接近 iOS 的观感。
 */
QPainterPath path(const QRectF &rect, qreal radius, qreal exponent = 4.0);

} // namespace Squircle

/** 用连续曲率圆角绘制的卡片（替代 QSS 的 border-radius） */
class SquircleCard : public QFrame
{
    Q_OBJECT

public:
    explicit SquircleCard(QWidget *parent = nullptr);

    void setRadius(qreal radius);
    void setFill(const QColor &color);
    void setFillGradient(const QColor &top, const QColor &bottom);   // 上下渐变填充
    void setBorder(const QColor &color, qreal width = 1.0);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_radius = 16.0;
    QColor m_fill = QColor(0x17, 0x18, 0x1B);
    QColor m_fillBottom;   // 有效时用上下渐变
    QColor m_border = QColor(0x2A, 0x2C, 0x30);
    qreal m_borderWidth = 1.0;
};

/** 用连续曲率圆角绘制的分组框：保留 QGroupBox 的标题与布局 */
class SquircleGroupBox : public QGroupBox
{
    Q_OBJECT

public:
    explicit SquircleGroupBox(const QString &title, QWidget *parent = nullptr);

    void setRadius(qreal radius);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void ensureBackdrop();                 // 抓取卡片背后的画面并模糊
    qreal m_radius = 16.0;
    QImage m_backdrop;
    QSize m_backdropSize;
};

#endif // SQUIRCLE_H
