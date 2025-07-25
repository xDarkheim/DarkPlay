#include "media/MediaManager.h"
#include "media/IMediaEngine.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>

namespace DarkPlay::Media {

// Qt MediaPlayer engine implementation
class QtMediaEngine : public IMediaEngine
{
    Q_OBJECT

public:
    explicit QtMediaEngine(QObject* parent = nullptr)
        : IMediaEngine(parent)
        , m_player(std::make_unique<QMediaPlayer>(this))
        , m_audioOutput(std::make_unique<QAudioOutput>(this))
        , m_videoSink(nullptr)
    {
        m_player->setAudioOutput(m_audioOutput.get());

        // Suppress FFmpeg verbose logging for AAC decoder
        qputenv("QT_LOGGING_RULES", "qt.multimedia.ffmpeg.debug=false");

        // Fix signal connections - use lambda to handle parameter mismatch
        connect(m_player.get(), &QMediaPlayer::playbackStateChanged,
                this, &QtMediaEngine::onStateChanged);
        connect(m_player.get(), &QMediaPlayer::mediaStatusChanged,
               this, &QtMediaEngine::onMediaStatusChanged);
        connect(m_player.get(), &QMediaPlayer::errorChanged,
                this, [this]() {
                    // Get the current error from the player
                    onErrorOccurred(m_player->error());
                });
        connect(m_player.get(), &QMediaPlayer::positionChanged,
                this, &IMediaEngine::positionChanged);
        connect(m_player.get(), &QMediaPlayer::durationChanged,
                this, &IMediaEngine::durationChanged);

        qDebug() << "QtMediaEngine initialized successfully";
    }

    // IMediaEngine implementation with proper [[nodiscard]] attributes
    void setSource(const QUrl& url) override {
        try {
            if (!url.isValid()) {
                emit errorOccurred("Invalid URL provided");
                return;
            }

            m_player->setSource(url);
            emit sourceChanged(url);
        } catch (const std::exception& e) {
            emit errorOccurred(QString("Failed to set source: %1").arg(e.what()));
        }
    }

    [[nodiscard]] QUrl source() const override { return m_player->source(); }

    void play() override {
        try {
            if (m_player->source().isEmpty()) {
                emit errorOccurred("No media source available");
                return;
            }
            m_player->play();
        } catch (const std::exception& e) {
            emit errorOccurred(QString("Playback failed: %1").arg(e.what()));
        }
    }

    void pause() override { m_player->pause(); }
    void stop() override { m_player->stop(); }

    [[nodiscard]] qint64 position() const override { return m_player->position(); }
    void setPosition(qint64 position) override { m_player->setPosition(position); }
    [[nodiscard]] qint64 duration() const override { return m_player->duration(); }

    [[nodiscard]] float volume() const override { return m_audioOutput->volume(); }
    void setVolume(float volume) override {
        try {
            if (volume < 0.0f || volume > 1.0f) {
                qWarning() << "Volume out of range, clamping to [0.0, 1.0]";
                volume = qBound(0.0f, volume, 1.0f);
            }
            m_audioOutput->setVolume(volume);
            emit volumeChanged(volume);
        } catch (const std::exception& e) {
            emit errorOccurred(QString("Volume control failed: %1").arg(e.what()));
        }
    }
    [[nodiscard]] bool isMuted() const override { return m_audioOutput->isMuted(); }
    void setMuted(bool muted) override { m_audioOutput->setMuted(muted); }

    // Fix narrowing conversion warning
    [[nodiscard]] float playbackRate() const override {
        return static_cast<float>(m_player->playbackRate());
    }
    void setPlaybackRate(float rate) override { m_player->setPlaybackRate(static_cast<qreal>(rate)); }

    [[nodiscard]] State state() const override {
        switch (m_player->playbackState()) {
            case QMediaPlayer::StoppedState: return State::Stopped;
            case QMediaPlayer::PlayingState: return State::Playing;
            case QMediaPlayer::PausedState: return State::Paused;
        }
        return State::Stopped;
    }

    [[nodiscard]] MediaStatus mediaStatus() const override {
        switch (m_player->mediaStatus()) {
            case QMediaPlayer::NoMedia: return MediaStatus::NoMedia;
            case QMediaPlayer::LoadingMedia: return MediaStatus::Loading;
            case QMediaPlayer::LoadedMedia: return MediaStatus::Loaded;
            case QMediaPlayer::BufferingMedia: return MediaStatus::Buffering;
            case QMediaPlayer::BufferedMedia: return MediaStatus::Buffered;
            case QMediaPlayer::EndOfMedia: return MediaStatus::EndOfMedia;
            case QMediaPlayer::InvalidMedia: return MediaStatus::InvalidMedia;
            default: return MediaStatus::Unknown;
        }
    }

    [[nodiscard]] bool isAvailable() const override { return m_player->isAvailable(); }

    [[nodiscard]] QStringList supportedMimeTypes() const override {
        return QStringList() << "video/mp4" << "video/avi" << "video/x-msvideo"
                            << "video/quicktime" << "video/x-ms-wmv" << "video/x-matroska"
                            << "audio/mpeg" << "audio/wav" << "audio/flac" << "audio/ogg";
    }

    [[nodiscard]] bool canPlay(const QUrl& url) const override {
        QString suffix = QFileInfo(url.toLocalFile()).suffix().toLower();
        QStringList supportedExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "mp3", "wav", "flac", "ogg"};
        return supportedExtensions.contains(suffix);
    }

    // Video output implementation
    void setVideoSink(QVideoSink* sink) override {
        m_videoSink = sink;
        if (m_player && m_videoSink) {
            m_player->setVideoSink(m_videoSink);
        }
    }

    [[nodiscard]] QVideoSink* videoSink() const override {
        return m_videoSink;
    }

private slots:
    void onStateChanged(QMediaPlayer::PlaybackState state) {
        Q_UNUSED(state)
        emit stateChanged(this->state());
    }

    void onMediaStatusChanged(QMediaPlayer::MediaStatus status) {
        Q_UNUSED(status)
        emit mediaStatusChanged(mediaStatus());
    }

    void onErrorOccurred(QMediaPlayer::Error error) {
        QString errorString;
        switch (error) {
            case QMediaPlayer::NoError:
                return; // No error, nothing to emit
            case QMediaPlayer::ResourceError:
                errorString = "Resource error: Could not resolve media resource";
                break;
            case QMediaPlayer::FormatError:
                errorString = "Format error: Unsupported media format";
                break;
            case QMediaPlayer::NetworkError:
                errorString = "Network error: Network connection failed";
                break;
            case QMediaPlayer::AccessDeniedError:
                errorString = "Access denied: Insufficient permissions";
                break;
            default:
                errorString = "Unknown media player error";
                break;
        }

        // Filter out common FFmpeg AAC warnings that are not critical
        if (errorString.contains("env_facs_q") || errorString.contains("AAC")) {
            qDebug() << "Non-critical audio warning:" << errorString;
            return; // Don't emit these as errors
        }

        emit errorOccurred(errorString);
    }

private:
    std::unique_ptr<QMediaPlayer> m_player;
    std::unique_ptr<QAudioOutput> m_audioOutput;
    QVideoSink* m_videoSink; // Not owned by this class
};

// Qt MediaPlayer factory with proper [[nodiscard]] attributes
class QtMediaEngineFactory : public IMediaEngineFactory
{
public:
    [[nodiscard]] QString name() const override { return "qt"; }
    [[nodiscard]] QString description() const override { return "Qt Multimedia Engine"; }
    [[nodiscard]] int priority() const override { return 100; }

    [[nodiscard]] QStringList supportedMimeTypes() const override {
        return QStringList() << "video/mp4" << "video/avi" << "video/x-msvideo"
                            << "video/quicktime" << "video/x-ms-wmv" << "video/x-matroska"
                            << "audio/mpeg" << "audio/wav" << "audio/flac" << "audio/ogg";
    }

    IMediaEngine* createEngine(QObject* parent) override {
        return new QtMediaEngine(parent);
    }

    [[nodiscard]] bool isAvailable() const override { return true; }
};

MediaManager::MediaManager(QObject* parent)
    : QObject(parent)
{
    setupDefaultEngines();
}

MediaManager::~MediaManager() = default;

void MediaManager::registerEngineFactory(std::unique_ptr<IMediaEngineFactory> factory)
{
    if (!factory) return;

    m_engineFactories.push_back(std::move(factory));

    // Sort by priority (higher priority first)
    std::ranges::sort(m_engineFactories,
              [](const auto& a, const auto& b) {
                  return a->priority() > b->priority();
              });

    emit supportedFormatsChanged();
}

QStringList MediaManager::availableEngines() const
{
    QStringList engines;
    for (const auto& factory : m_engineFactories) {
        if (factory->isAvailable()) {
            engines.append(factory->name());
        }
    }
    return engines;
}

IMediaEngine* MediaManager::createEngine(const QString& engineName) const
{
    if (engineName.isEmpty()) {
        // Return the highest priority available engine
        for (const auto& factory : m_engineFactories) {
            if (factory->isAvailable()) {
                return factory->createEngine(nullptr);
            }
        }
        return nullptr;
    }

    // Find specific engine
    for (const auto& factory : m_engineFactories) {
        if (factory->name() == engineName && factory->isAvailable()) {
            return factory->createEngine(nullptr);
        }
    }

    return nullptr;
}

IMediaEngine* MediaManager::createEngineForUrl(const QUrl& url) const
{
    IMediaEngineFactory* bestFactory = findBestEngine(url);
    return bestFactory ? bestFactory->createEngine(nullptr) : nullptr;
}

void MediaManager::setCurrentEngine(std::unique_ptr<IMediaEngine> engine)
{
    m_currentEngine = std::move(engine);
    emit engineChanged(m_currentEngine.get());
}

QStringList MediaManager::supportedFormats() const
{
    QStringList formats;
    for (const auto& factory : m_engineFactories) {
        if (factory->isAvailable()) {
            formats.append(factory->supportedMimeTypes());
        }
    }
    formats.removeDuplicates();
    return formats;
}

bool MediaManager::canPlay(const QUrl& url) const
{
    return findBestEngine(url) != nullptr;
}

void MediaManager::setupDefaultEngines()
{
    // Register Qt Multimedia engine
    registerEngineFactory(std::make_unique<QtMediaEngineFactory>());
}

IMediaEngineFactory* MediaManager::findBestEngine(const QUrl& url) const
{
    for (const auto& factory : m_engineFactories) {
        if (factory->isAvailable()) {
            auto engine = std::unique_ptr<IMediaEngine>(factory->createEngine(nullptr));
            if (engine && engine->canPlay(url)) {
                return factory.get();
            }
        }
    }
    return nullptr;
}

} // namespace DarkPlay

#include "MediaManager.moc"
