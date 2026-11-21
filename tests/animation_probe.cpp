// 动画诊断：滚轮缓动 + 感叹号提示淡入
#include "widgets/smoothscroller.h"
#include "widgets/typewritertooltip.h"

#include <QApplication>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

int main(int argc, char *argv[])
{
    KopalApplication app(argc, argv);
    QTextStream out(stdout);

    QScrollArea area;
    QWidget content;
    auto *layout = new QVBoxLayout(&content);
    for (int i = 0; i < 80; ++i)
        layout->addWidget(new QLabel(QStringLiteral("row %1").arg(i), &content));
    area.setWidget(&content);
    area.setWidgetResizable(true);
    area.resize(320, 220);
    area.show();
    app.processEvents();

    QScrollBar *bar = area.verticalScrollBar();
    out << "range=" << bar->minimum() << ".." << bar->maximum() << "\n";

    const QPoint pos = area.viewport()->rect().center();
    QWheelEvent wheel(QPointF(pos), QPointF(area.viewport()->mapToGlobal(pos)),
                      QPoint(0, 0), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(area.viewport(), &wheel);
    out << "immediate=" << bar->value() << "\n";
    out.flush();

    QTimer::singleShot(320, &app, [&]() {
        out << "after=" << bar->value() << "\n";
        const bool scrolled = bar->value() > 0;

        TypewriterTooltip *tip = TypewriterTooltip::instance();
        tip->showText(QStringLiteral("这是一个很长的提示，用来验证淡入淡出。"), &area);
        const qreal early = tip->windowOpacity();
        QTimer::singleShot(260, &app, [&, early, scrolled, tip]() {
            const qreal late = tip->windowOpacity();
            out << "tooltip early=" << early << " late=" << late << "\n";
            const bool faded = early < 0.9 && late > 0.9;
            out << (scrolled && faded ? "PASS" : "FAIL") << "\n";
            app.exit(scrolled && faded ? 0 : 1);
        });
    });

    return app.exec();
}
