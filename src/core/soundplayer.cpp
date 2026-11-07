#include "soundplayer.h"

#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <mmsystem.h>
#endif

namespace AppSound {

bool playWav(const QString &path)
{
#ifdef Q_OS_WIN
    if (path.isEmpty())
        return false;

    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return false;

    const std::wstring native = QDir::toNativeSeparators(info.absoluteFilePath()).toStdWString();
    return PlaySoundW(native.c_str(), nullptr,
                      SND_FILENAME | SND_ASYNC | SND_NODEFAULT) != FALSE;
#else
    Q_UNUSED(path)
    return false;
#endif
}

void stop()
{
#ifdef Q_OS_WIN
    PlaySoundW(nullptr, nullptr, 0);
#endif
}

} // namespace AppSound
