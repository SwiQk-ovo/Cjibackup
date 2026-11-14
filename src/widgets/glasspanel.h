#ifndef GLASSPANEL_H
#define GLASSPANEL_H

#include <QImage>
#include <QPainter>
#include <QWidget>

class QTimer;

/**
 * 液态玻璃面板。
 *
 * 在深色界面里做「玻璃」不需要真的去模糊背景（背后本来就是纯色渐变），
 * 靠这几层叠出来更像：
 *   1) 半透明白色底  rgba(255,255,255,0.055)
 *   2) 顶部高光渐变  白 0.16 → 透明（模拟光从上方掠过）
 *   3) 1px 渐变描边  上亮下暗，制造厚度
 *   4) 内侧一圈柔和暗边（玻璃边缘的折射感）
 *   5) 可选：缓慢流动的斜向高光（液态感）
 */
class GlassPanel : public QWidget
{
    Q_OBJECT

public:
    // 供别的控件复用：把 backdrop（背后画面）折射后画到 rect 里
    static void paintGlass(QPainter *p, const QRectF &rect, const QImage &backdrop,
                           qreal radius, int refractionPx, const QColor &tint,
                           qreal sheenPhase, qreal sheenStrength);
    explicit GlassPanel(QWidget *parent = nullptr);

    void setRadius(qreal radius);
    void setRefraction(int pixels);   // 折射强度（像素），0 = 关闭
    void setTint(const QColor &color);
    void setSheenEnabled(bool enabled);
    void setSheenStrength(qreal strength);   // 0.0 - 1.0

    // 让面板内容带内边距（供 layout 用）
    void setContentMargins(int left, int top, int right, int bottom);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void tickSheen();
    QImage buildRefracted();

    QImage m_cache;
    int m_refraction = 12;

    qreal m_radius = 16.0;
    QColor m_tint{0xFF, 0xFF, 0xFF, 22};
    bool m_sheen = true;
    qreal m_sheenStrength = 0.10;
    qreal m_phase = 0.0;
    QTimer *m_timer = nullptr;
};

#endif // GLASSPANEL_H
