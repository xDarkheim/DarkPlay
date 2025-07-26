#ifndef DARKPLAY_MEDIA_MEDIAMANAGER_H
#define DARKPLAY_MEDIA_MEDIAMANAGER_H

#include <QObject>
#include <QUrl>
#include <QStringList>
#include <QTimer>
#include <memory>
#include <mutex>
#include "IMediaEngine.h"

namespace DarkPlay::Media {

class MediaManager : public QObject {
    Q_OBJECT

public:
    explicit MediaManager(QObject* parent = nullptr);
    ~MediaManager() override;

    // Engine management
    void setMediaEngine(std::unique_ptr<IMediaEngine> engine);
    [[nodiscard]] IMediaEngine* mediaEngine() const;

    // Playback control
    bool loadMedia(const QUrl& url);
    void play();
    void pause();
    void stop();
    void togglePlayPause();

    // Position and seeking
    [[nodiscard]] qint64 position() const;
    [[nodiscard]] qint64 duration() const;
    void setPosition(qint64 position);
    void seek(qint64 offset);
    void seekForward(qint64 seconds = 10);
    void seekBackward(qint64 seconds = 10);

    // Volume control
    [[nodiscard]] int volume() const;
    void setVolume(int volume);
    void increaseVolume(int step = 5);
    void decreaseVolume(int step = 5);
    [[nodiscard]] bool isMuted() const;
    void setMuted(bool muted);
    void toggleMute();

    // Playback rate
    [[nodiscard]] qreal playbackRate() const;
    void setPlaybackRate(qreal rate);
    void increaseSpeed();
    void decreaseSpeed();
    void resetSpeed();

    // State and information
    [[nodiscard]] PlaybackState state() const;
    [[nodiscard]] MediaType mediaType() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] QString currentMediaUrl() const;

    // Media information
    [[nodiscard]] QString title() const;
    [[nodiscard]] QSize videoSize() const;
    [[nodiscard]] bool hasVideo() const;
    [[nodiscard]] bool hasAudio() const;

    // Playlist support
    void setPlaylist(const QStringList& urls);
    [[nodiscard]] QStringList playlist() const;
    [[nodiscard]] int currentIndex() const;
    void setCurrentIndex(int index);
    void next();
    void previous();
    [[nodiscard]] bool hasNext() const;
    [[nodiscard]] bool hasPrevious() const;

    // Auto-play settings
    void setAutoPlay(bool enabled);
    [[nodiscard]] bool autoPlay() const;
    void setRepeatMode(bool enabled);
    [[nodiscard]] bool repeatMode() const;

    // Thread-safe engine access
    [[nodiscard]] bool hasEngine() const noexcept;

signals:
    void mediaLoaded(const QString& url);
    void stateChanged(PlaybackState state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void playbackRateChanged(qreal rate);
    void mediaInfoChanged();
    void errorOccurred(const QString& error);
    void playlistChanged();
    void currentIndexChanged(int index);

private slots:
    void onPositionTimer();
    void onEngineStateChanged(PlaybackState state);
    void onEnginePositionChanged(qint64 position);
    void onEngineDurationChanged(qint64 duration);
    void onEngineVolumeChanged(int volume);
    void onEngineMutedChanged(bool muted);
    void onEnginePlaybackRateChanged(qreal rate);
    void onEngineMediaInfoChanged();
    void onEngineErrorOccurred(const QString& error);

private:
    void connectEngineSignals();
    void disconnectEngineSignals();
    void validatePlaylistIndex();
    void loadCurrentMedia();
    [[nodiscard]] bool isValidIndex(int index) const;

    // Thread-safe engine operations
    template<typename Func>
    auto safeEngineCall(Func&& func) const -> decltype(func(std::declval<IMediaEngine&>()));

    template<typename Func>
    bool safeEngineCallVoid(Func&& func) const;

    std::unique_ptr<IMediaEngine> m_engine;
    mutable std::recursive_mutex m_engineMutex; // Protect engine access

    QStringList m_playlist;
    int m_currentIndex;
    QString m_currentUrl;
    bool m_autoPlay;
    bool m_repeatMode;
    int m_previousVolume;

    QTimer* m_positionTimer;
    mutable std::mutex m_playlistMutex; // Protect playlist operations
};

} // namespace DarkPlay::Media

#endif // DARKPLAY_MEDIA_MEDIAMANAGER_H
