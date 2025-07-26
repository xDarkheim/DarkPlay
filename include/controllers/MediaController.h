#ifndef DARKPLAY_CONTROLLERS_MEDIACONTROLLER_H
#define DARKPLAY_CONTROLLERS_MEDIACONTROLLER_H

#include <QObject>
#include <QUrl>
#include <QTimer>
#include <QVideoSink>
#include <memory>
#include "media/MediaManager.h"
#include "media/IMediaEngine.h"

namespace DarkPlay::Controllers {

/**
 * @brief High-level controller for media operations
 * Acts as a bridge between UI and MediaManager
 */
class MediaController : public QObject
{
    Q_OBJECT

public:
    explicit MediaController(QObject* parent = nullptr);
    ~MediaController() override;

    // Media Manager access
    [[nodiscard]] Media::MediaManager* mediaManager() const { return m_mediaManager.get(); }

    // High-level playback control
    bool openFile(const QString& filePath);
    bool openUrl(const QUrl& url);
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // Seeking and position
    void seek(qint64 position);
    void seekRelative(qint64 offset);
    [[nodiscard]] qint64 position() const;
    [[nodiscard]] qint64 duration() const;

    // Volume control
    void setVolume(int volume);
    [[nodiscard]] int volume() const;
    void setMuted(bool muted);
    [[nodiscard]] bool isMuted() const;

    // Playback rate
    void setPlaybackRate(qreal rate);
    [[nodiscard]] qreal playbackRate() const;

    // State information
    [[nodiscard]] Media::PlaybackState state() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] bool hasMedia() const;

    // Media information
    [[nodiscard]] QString currentMediaUrl() const;
    [[nodiscard]] QString title() const;
    [[nodiscard]] QSize videoSize() const;
    [[nodiscard]] bool hasVideo() const;
    [[nodiscard]] bool hasAudio() const;

    // Video output support
    void setVideoSink(QVideoSink* sink);
    [[nodiscard]] QVideoSink* videoSink() const;

public slots:
    // Convenience slots for UI binding
    void onPlayRequested();
    void onPauseRequested();
    void onStopRequested();
    void onVolumeChangeRequested(int volume);
    void onSeekRequested(qint64 position);

signals:
    // High-level signals for UI
    void mediaOpened(const QString& url);
    void mediaLoadFailed(const QString& error);
    void playbackStateChanged(Media::PlaybackState state);
    void stateChanged(Media::PlaybackState state);  // Alias for playbackStateChanged
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void playbackRateChanged(qreal rate);
    void mediaInfoChanged();
    void errorOccurred(const QString& error);

private slots:
    // MediaManager signal handlers
    void onManagerStateChanged(Media::PlaybackState state);
    void onManagerPositionChanged(qint64 position);
    void onManagerDurationChanged(qint64 duration);
    void onManagerVolumeChanged(int volume);
    void onManagerMutedChanged(bool muted);
    void onManagerPlaybackRateChanged(qreal rate);
    void onManagerMediaLoaded(const QString& url);
    void onManagerError(const QString& error);

private:
    void setupConnections();
    void initializeDefaultEngine();  // Remove static keyword

    std::unique_ptr<Media::MediaManager> m_mediaManager;
    QString m_lastError;
};

} // namespace DarkPlay::Controllers

#endif // DARKPLAY_CONTROLLERS_MEDIACONTROLLER_H
