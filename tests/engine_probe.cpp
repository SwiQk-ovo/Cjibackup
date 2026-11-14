// 临时探针：拿真实的测试环境跑一遍 BackupEngine，看它到底有没有复制、失败原因是什么
#include "core/backupengine.h"
#include "core/settingsmanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>
#include <QTimer>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QStringList args = app.arguments();
    if (args.size() < 3) {
        QTextStream(stdout) << "用法: engine_probe <源> <目标>\n";
        return 2;
    }

    auto *engine = new BackupEngine(&app);
    QObject::connect(engine, &BackupEngine::stateChanged, [](BackupEngine::State s) {
        QTextStream(stdout) << "state=" << int(s) << "\n";
    });
    QObject::connect(engine, &BackupEngine::progressUpdated, [](const BackupStats &st) {
        QTextStream(stdout) << "progress files=" << st.copiedFiles << "/" << st.totalFiles
                            << " failed=" << st.failedCount << " bytes=" << st.totalBytes << "\n";
    });
    QObject::connect(engine, &BackupEngine::errorOccurred, [](const QString &msg) {
        QTextStream(stdout) << "ERROR: " << msg << "\n";
    });
    QObject::connect(engine, &BackupEngine::completed, [&]() {
        const BackupStats st = engine->stats();
        const VerifyResult v = engine->verifyResult();
        QTextStream(stdout) << "DONE copied=" << st.copiedFiles << "/" << st.totalFiles
                            << " failed=" << st.failedCount
                            << " verifyRan=" << v.ran << " checked=" << v.checked
                            << " missing=" << v.missing << "\n";
        QCoreApplication::quit();
    });

    QTimer::singleShot(0, [&]() { engine->start(args.at(1), args.at(2)); });
    QTimer::singleShot(120000, [&]() {
        QTextStream(stdout) << "TIMEOUT\n";
        QCoreApplication::quit();
    });
    return app.exec();
}
