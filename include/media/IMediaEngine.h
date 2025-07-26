#ifndef DARKPLAY_MEDIA_IMEDIAENGINE_H
#define DARKPLAY_MEDIA_IMEDIAENGINE_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QSize>

namespace DarkPlay::Media {

enum class PlaybackState {
    Stopped,
    Playing,
    Paused,
    Buffering,
    Error
};

enum class MediaType {
    Video,
    Audio,
    Unknown
};

class IMediaEngine : public QObject {
    Q_OBJECT

public:
    explicit IMediaEngine(QObject* parent = nullptr) : QObject(parent) {}
    ~IMediaEngine() override = default;

    // Core playback methods
    virtual bool loadMedia(const QUrl& url) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

    // Position and duration
    [[nodiscard]] virtual qint64 position() const = 0;
    [[nodiscard]] virtual qint64 duration() const = 0;
    virtual void setPosition(qint64 position) = 0;

    // Volume control
    [[nodiscard]] virtual int volume() const = 0;
    virtual void setVolume(int volume) = 0;
    [[nodiscard]] virtual bool isMuted() const = 0;
    virtual void setMuted(bool muted) = 0;

    // Playback rate
    [[nodiscard]] virtual qreal playbackRate() const = 0;
    virtual void setPlaybackRate(qreal rate) = 0;

    // State information
    [[nodiscard]] virtual PlaybackState state() const = 0;
    [[nodiscard]] virtual MediaType mediaType() const = 0;
    [[nodiscard]] virtual QString errorString() const = 0;

    // Media information
    [[nodiscard]] virtual QString title() const = 0;
    [[nodiscard]] virtual QSize videoSize() const = 0;
    [[nodiscard]] virtual bool hasVideo() const = 0;
    [[nodiscard]] virtual bool hasAudio() const = 0;

signals:
    void stateChanged(PlaybackState state);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(int volume);
    void mutedChanged(bool muted);
    void playbackRateChanged(qreal rate);
    void mediaLoaded();
    void error(const QString& errorString);
    void bufferingProgress(int progress);
};

} // namespace DarkPlay::Media

Q_DECLARE_METATYPE(DarkPlay::Media::PlaybackState)
Q_DECLARE_METATYPE(DarkPlay::Media::MediaType)

#endif // DARKPLAY_MEDIA_IMEDIAENGINE_H
