#include "localization.h"

#include <QApplication>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QStringList>
#include <QTranslator>

namespace Localization {

void installChineseTranslations(QApplication &app)
{
    const QStringList dirs{
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../translations"),
        QLibraryInfo::path(QLibraryInfo::TranslationsPath)};

    for (const QString &dir : dirs) {
        auto *translator = new QTranslator(&app);
        if (translator->load(QStringLiteral("qtbase_zh_CN"), dir)) {
            app.installTranslator(translator);
            return;
        }
        delete translator;
    }
}

} // namespace Localization
