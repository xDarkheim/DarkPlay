#ifndef DARKPLAY_MEDIA_IMEDIAENGINE_H
#define DARKPLAY_MEDIA_IMEDIAENGINE_H

#include <QObject>
#include <QUrl>
#include <QMediaPlayer>
#include <QVideoSink>

namespace DarkPlay::Media {

/**
 * @brief Abstract interface for media playbook engines
 */
class IMediaEngine : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Playing,
        Paused,
        Buffering,
        Error
    };

    enum class MediaStatus {
        Unknown,
        NoMedia,
        Loading,
        Loaded,
        Buffering,
        Buffered,
        EndOfMedia,
        InvalidMedia
    };

    explicit IMediaEngine(QObject* parent = nullptr) : QObject(parent) {}
    ~IMediaEngine() override = default;

    // Media control
    virtual void setSource(const QUrl& url) = 0;
    [[nodiscard]] virtual QUrl source() const = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

    // Position and seeking
    [[nodiscard]] virtual qint64 position() const = 0;
    virtual void setPosition(qint64 position) = 0;
    [[nodiscard]] virtual qint64 duration() const = 0;

    // Volume control
    [[nodiscard]] virtual float volume() const = 0;
    virtual void setVolume(float volume) = 0;
    [[nodiscard]] virtual bool isMuted() const = 0;
    virtual void setMuted(bool muted) = 0;

    // Playback rate
    [[nodiscard]] virtual float playbackRate() const = 0;
    virtual void setPlaybackRate(float rate) = 0;

    // State queries
    [[nodiscard]] virtual State state() const = 0;
    [[nodiscard]] virtual MediaStatus mediaStatus() const = 0;
    [[nodiscard]] virtual bool isAvailable() const = 0;

    // Supported formats
    [[nodiscard]] virtual QStringList supportedMimeTypes() const = 0;
    [[nodiscard]] virtual bool canPlay(const QUrl& url) const = 0;

    // Video output
    virtual void setVideoSink(QVideoSink* sink) = 0;
    [[nodiscard]] virtual QVideoSink* videoSink() const = 0;

signals:
    void sourceChanged(const QUrl& url);
    void stateChanged(State state);
    void mediaStatusChanged(MediaStatus status);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void volumeChanged(float volume);
    void mutedChanged(bool muted);
    void playbackRateChanged(float rate);
    void errorOccurred(const QString& error);
    void bufferProgressChanged(float progress);
};

/**
 * @brief Factory interface for creating media engines
 */
class IMediaEngineFactory
{
public:
    virtual ~IMediaEngineFactory() = default;

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QString description() const = 0;
    [[nodiscard]] virtual int priority() const = 0;
    [[nodiscard]] virtual QStringList supportedMimeTypes() const = 0;

    virtual IMediaEngine* createEngine(QObject* parent) = 0;
    [[nodiscard]] virtual bool isAvailable() const = 0;
};

} // namespace DarkPlay::Media

Q_DECLARE_METATYPE(DarkPlay::Media::IMediaEngine::State)
Q_DECLARE_METATYPE(DarkPlay::Media::IMediaEngine::MediaStatus)

#endif // DARKPLAY_MEDIA_IMEDIAENGINE_H
