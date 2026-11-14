#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QImage>
#include <QListWidget>
#include <QStringList>
#include <QWidget>

class QLabel;
class QMediaPlayer;
class QAudioOutput;
class QVideoSink;
class QTimer;
class ControlBar;

/**
 * 内置播放器窗口。
 *
 * 优先用 libmpv（运行时动态加载 mpv-2.dll / libmpv-2.dll），
 * 找不到就退回 Qt Multimedia（QMediaPlayer + QVideoWidget）。
 * 控制条完全自绘，照片直接用 QLabel 显示。
 */
class MediaPlayerWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MediaPlayerWindow(const QString &path, QWidget *parent = nullptr);
    ~MediaPlayerWindow() override;

    void setPlaylist(const QStringList &files, int current);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTick();

private:
    void buildUi();
    void openPath(const QString &path);
    void openImage(const QString &path);
    void openVideo(const QString &path);
    void togglePlay();
    void step(const int deltaSeconds);
    void stepFrame(int deltaFrames);
    void applyRate(qreal rate);
    void cycleRate();
    void setVolume(int volume);
    void toggleFullScreen();
    void showContextMenu(const QPoint &globalPos);
    void takeScreenshot();
    void showProperties();
    void playRelative(int delta);
    void updateFromBackend();
    void showHint(const QString &text);

    // libmpv
    bool initMpv();
    void mpvCommand(const QStringList &args);
    void shutdownMpv();

    QString m_path;
    QStringList m_playlist;
    QListWidget *m_list = nullptr;
    int m_index = -1;
    bool m_isImage = false;
    bool m_playing = false;
    bool m_fullscreen = false;
    qreal m_rate = 1.0;
    qint64 m_duration = 0;
    qint64 m_position = 0;

    QWidget *m_videoArea = nullptr;
    QLabel *m_imageLabel = nullptr;
    ControlBar *m_bar = nullptr;
    QLabel *m_hint = nullptr;
    QTimer *m_timer = nullptr;
    QTimer *m_hideTimer = nullptr;

    // Qt Multimedia 后端
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    QVideoSink *m_sink = nullptr;
    QWidget *m_videoWidget = nullptr;

    // libmpv 后端
    void *m_mpv = nullptr;
    QImage m_frame;   // 当前视频帧，给控制条做折射背景
    void *m_mpvLib = nullptr;
    bool m_useMpv = false;
    int m_volume = 80;
};

#endif // MEDIAPLAYER_H
