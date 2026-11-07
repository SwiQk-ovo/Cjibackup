#ifndef SOUNDPLAYER_H
#define SOUNDPLAYER_H

#include <QString>

namespace AppSound {

// 用 Windows 自带 PlaySound 播放 WAV，无需 Qt 多媒体模块
bool playWav(const QString &path);
void stop();

} // namespace AppSound

#endif // SOUNDPLAYER_H
