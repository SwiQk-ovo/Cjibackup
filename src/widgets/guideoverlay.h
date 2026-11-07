#ifndef GUIDEOVERLAY_H
#define GUIDEOVERLAY_H

#include <QWidget>
#include <QVariantAnimation>

#include <functional>

class QLabel;
class QPushButton;
class QPixmap;
class QTimer;

/**
 * 首次启动的使用引导（spotlight 式）：
 * 半透明遮罩盖住主窗口，在当前讲解的控件位置开一个"洞"，
 * 旁边浮一张卡片写明「第 N 步，共 M 步」+ 标题 + 说明 + 上一步/下一步/跳过。
 *
 * 每一步可以做两件事：
 *   prepare()  切到对应页面、把目标滚动到可见
 *   target()   返回要高亮的控件（返回空则高亮整个窗口）
 */
class GuideOverlay : public QWidget
{
    Q_OBJECT

public:
    struct Step
    {
        QString title;
        QString text;
        std::function<QWidget *()> target;
        std::function<void()> prepare;
    };

    explicit GuideOverlay(QWidget *host);

    void start(const QVector<Step> &steps);
    void stop();

    bool isActive() const { return m_active; }

    // 主窗口尺寸变了以后重新排版
    void relayout();

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setStep(int index);
    void captureTarget();
    void updateCard();
    void layoutCard();
    void finish();

    QWidget *m_host = nullptr;
    QVector<Step> m_steps;
    int m_index = 0;
    bool m_active = false;

    QWidget *m_card = nullptr;
    QLabel *m_stepLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_bodyLabel = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_skipBtn = nullptr;

    QRect m_holeRect;
    QPixmap m_holePixmap;
    int m_pending = 0;

    QVariantAnimation *m_fadeAnim = nullptr;
    qreal m_fade = 1.0;
};

#endif // GUIDEOVERLAY_H
