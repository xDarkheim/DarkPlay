#ifndef DARKPLAY_CONTROLLERS_MEDIACONTROLLER_H
#define DARKPLAY_CONTROLLERS_MEDIACONTROLLER_H

#include <QTimer>
#include <QPointer>
#include <memory>

// Include full header instead of forward declaration for enums
#include "media/IMediaEngine.h"
#include "media/MediaManager.h"

class QVideoSink;

namespace DarkPlay::Controllers {

/**
 * @brief Controller for managing media playback with proper RAII and exception safety
 */
class MediaController : public QObject
{
    Q_OBJECT

public:
    explicit MediaController(QObject* parent = nullptr);
    ~MediaController() override = default;

    // Delete copy and move operations for safety
    MediaController(const MediaController&) = delete;
    MediaController& operator=(const MediaController&) = delete;
    MediaController(MediaController&&) = delete;
    MediaController& operator=(MediaController&&) = delete;

    // Media operations with exception safety
    bool loadMedia(const QString& filePath) noexcept;
    void play() noexcept;
    void pause() noexcept;
    void stop() noexcept;
    void seek(qint64 position) noexcept;
    void setVolume(float volume) noexcept;

    // Video output
    void setVideoSink(QVideoSink* sink) noexcept;
    [[nodiscard]] QVideoSink* videoSink() const noexcept;

    // State queries - all noexcept
    [[nodiscard]] bool hasMedia() const noexcept;
    [[nodiscard]] qint64 position() const noexcept;
    [[nodiscard]] qint64 duration() const noexcept;
    [[nodiscard]] float volume() const noexcept;
    [[nodiscard]] Media::IMediaEngine::State state() const noexcept;

    // Engine management
    [[nodiscard]] bool isEngineAvailable() const noexcept { return static_cast<bool>(m_currentEngine); }

signals:
    void mediaLoaded(const QString& fileName);
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void stateChanged(Media::IMediaEngine::State state);
    void volumeChanged(float volume);
    void errorOccurred(const QString& error);

private slots:
    void onEnginePositionChanged(qint64 position);
    void onEngineDurationChanged(qint64 duration);
    void onEngineStateChanged(Media::IMediaEngine::State state);
    void onEngineError(const QString& error);

private:
    void connectEngineSignals() noexcept;
    void disconnectEngineSignals() noexcept;

    std::unique_ptr<Media::MediaManager> m_mediaManager;
    std::unique_ptr<Media::IMediaEngine> m_currentEngine;

    // Use QPointer for safe access to Qt objects
    QPointer<QVideoSink> m_videoSink;

    // State tracking
    QString m_currentFilePath;
    float m_currentVolume{0.7f};
};

} // namespace DarkPlay::Controllers

#endif // DARKPLAY_CONTROLLERS_MEDIACONTROLLER_H
