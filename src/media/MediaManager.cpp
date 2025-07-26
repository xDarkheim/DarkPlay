#include "media/MediaManager.h"
#include <QDebug>
#include <mutex>

namespace DarkPlay::Media {

MediaManager::MediaManager(QObject* parent)
    : QObject(parent)
    , m_currentIndex(-1)
    , m_autoPlay(false)
    , m_repeatMode(false)
    , m_previousVolume(50)
    , m_positionTimer(new QTimer(this))
{
    m_positionTimer->setInterval(100); // Update position every 100ms
    connect(m_positionTimer, &QTimer::timeout, this, &MediaManager::onPositionTimer);
}

MediaManager::~MediaManager() = default;

void MediaManager::setMediaEngine(std::unique_ptr<IMediaEngine> engine)
{
    std::lock_guard<std::recursive_mutex> lock(m_engineMutex);

    if (m_engine) {
        disconnectEngineSignals();
        m_engine->stop();
    }

    m_engine = std::move(engine);

    if (m_engine) {
        connectEngineSignals();
    }
}

IMediaEngine* MediaManager::mediaEngine() const
{
    std::lock_guard<std::recursive_mutex> lock(m_engineMutex);
    return m_engine.get();
}

bool MediaManager::hasEngine() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_engineMutex);
    return m_engine != nullptr;
}

bool MediaManager::loadMedia(const QUrl& url)
{
    return safeEngineCall([&](IMediaEngine& engine) -> bool {
        m_currentUrl = url.toString();
        bool success = engine.loadMedia(url);

        if (success) {
            emit mediaLoaded(m_currentUrl);
        }

        return success;
    });
}

void MediaManager::play()
{
    if (safeEngineCallVoid([](IMediaEngine& engine) {
        engine.play();
    })) {
        if (!m_positionTimer->isActive()) {
            m_positionTimer->start();
        }
    }
}

void MediaManager::pause()
{
    if (safeEngineCallVoid([](IMediaEngine& engine) {
        engine.pause();
    })) {
        m_positionTimer->stop();
    }
}

void MediaManager::stop()
{
    if (safeEngineCallVoid([](IMediaEngine& engine) {
        engine.stop();
    })) {
        m_positionTimer->stop();
    }
}

void MediaManager::togglePlayPause()
{
    safeEngineCallVoid([this](IMediaEngine& engine) {
        PlaybackState currentState = engine.state();
        if (currentState == PlaybackState::Playing) {
            engine.pause();
            m_positionTimer->stop();
        } else if (currentState == PlaybackState::Paused || currentState == PlaybackState::Stopped) {
            engine.play();
            if (!m_positionTimer->isActive()) {
                m_positionTimer->start();
            }
        }
    });
}

qint64 MediaManager::position() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> qint64 {
        return engine.position();
    });
}

qint64 MediaManager::duration() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> qint64 {
        return engine.duration();
    });
}

void MediaManager::setPosition(qint64 position)
{
    safeEngineCallVoid([position](IMediaEngine& engine) {
        engine.setPosition(position);
    });
}

void MediaManager::seek(qint64 offset)
{
    safeEngineCallVoid([this, offset](IMediaEngine& engine) {
        qint64 newPosition = engine.position() + offset;
        newPosition = qMax(0LL, qMin(newPosition, engine.duration()));
        engine.setPosition(newPosition);
    });
}

void MediaManager::seekForward(qint64 seconds)
{
    seek(seconds * 1000); // Convert seconds to milliseconds
}

void MediaManager::seekBackward(qint64 seconds)
{
    seek(-seconds * 1000); // Convert seconds to milliseconds
}

int MediaManager::volume() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> int {
        return engine.volume();
    });
}

void MediaManager::setVolume(int volume)
{
    safeEngineCallVoid([this, volume](IMediaEngine& engine) {
        int clampedVolume = qBound(0, volume, 100);
        engine.setVolume(clampedVolume);
        if (clampedVolume > 0) {
            m_previousVolume = clampedVolume;
        }
    });
}

void MediaManager::increaseVolume(int step)
{
    setVolume(volume() + step);
}

void MediaManager::decreaseVolume(int step)
{
    setVolume(volume() - step);
}

bool MediaManager::isMuted() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> bool {
        return engine.isMuted();
    });
}

void MediaManager::setMuted(bool muted)
{
    safeEngineCallVoid([muted](IMediaEngine& engine) {
        engine.setMuted(muted);
    });
}

void MediaManager::toggleMute()
{
    safeEngineCallVoid([this](IMediaEngine& engine) {
        if (engine.isMuted()) {
            engine.setMuted(false);
            if (engine.volume() == 0 && m_previousVolume > 0) {
                engine.setVolume(m_previousVolume);
            }
        } else {
            if (engine.volume() > 0) {
                m_previousVolume = engine.volume();
            }
            engine.setMuted(true);
        }
    });
}

qreal MediaManager::playbackRate() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> qreal {
        return engine.playbackRate();
    });
}

void MediaManager::setPlaybackRate(qreal rate)
{
    safeEngineCallVoid([rate](IMediaEngine& engine) {
        qreal clampedRate = qBound(0.25, rate, 4.0); // Limit playback rate
        engine.setPlaybackRate(clampedRate);
    });
}

void MediaManager::increaseSpeed()
{
    qreal currentRate = playbackRate();
    qreal newRate = currentRate + 0.25;
    setPlaybackRate(newRate);
}

void MediaManager::decreaseSpeed()
{
    qreal currentRate = playbackRate();
    qreal newRate = currentRate - 0.25;
    setPlaybackRate(newRate);
}

void MediaManager::resetSpeed()
{
    setPlaybackRate(1.0);
}

PlaybackState MediaManager::state() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> PlaybackState {
        return engine.state();
    });
}

MediaType MediaManager::mediaType() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> MediaType {
        return engine.mediaType();
    });
}

QString MediaManager::errorString() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> QString {
        return engine.errorString();
    });
}

QString MediaManager::currentMediaUrl() const
{
    return m_currentUrl;
}

QString MediaManager::title() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> QString {
        return engine.title();
    });
}

QSize MediaManager::videoSize() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> QSize {
        return engine.videoSize();
    });
}

bool MediaManager::hasVideo() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> bool {
        return engine.hasVideo();
    });
}

bool MediaManager::hasAudio() const
{
    return safeEngineCall([](const IMediaEngine& engine) -> bool {
        return engine.hasAudio();
    });
}

void MediaManager::setPlaylist(const QStringList& urls)
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    m_playlist = urls;
    m_currentIndex = urls.isEmpty() ? -1 : 0;
    emit playlistChanged();
    emit currentIndexChanged(m_currentIndex);

    if (!urls.isEmpty() && m_autoPlay) {
        loadCurrentMedia();
    }
}

QStringList MediaManager::playlist() const
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    return m_playlist;
}

int MediaManager::currentIndex() const
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    return m_currentIndex;
}

void MediaManager::setCurrentIndex(int index)
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    if (isValidIndex(index) && index != m_currentIndex) {
        m_currentIndex = index;
        emit currentIndexChanged(m_currentIndex);
        loadCurrentMedia();
    }
}

void MediaManager::next()
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    if (hasNext()) {
        setCurrentIndex(m_currentIndex + 1);
    } else if (m_repeatMode && !m_playlist.isEmpty()) {
        setCurrentIndex(0);
    }
}

void MediaManager::previous()
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    if (hasPrevious()) {
        setCurrentIndex(m_currentIndex - 1);
    } else if (m_repeatMode && !m_playlist.isEmpty()) {
        setCurrentIndex(static_cast<int>(m_playlist.size() - 1));
    }
}

bool MediaManager::hasNext() const
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    return m_currentIndex < m_playlist.size() - 1;
}

bool MediaManager::hasPrevious() const
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    return m_currentIndex > 0;
}

void MediaManager::setAutoPlay(bool enabled)
{
    m_autoPlay = enabled;
}

bool MediaManager::autoPlay() const
{
    return m_autoPlay;
}

void MediaManager::setRepeatMode(bool enabled)
{
    m_repeatMode = enabled;
}

bool MediaManager::repeatMode() const
{
    return m_repeatMode;
}

void MediaManager::onPositionTimer()
{
    safeEngineCallVoid([this](const IMediaEngine& engine) {
        if (engine.state() == PlaybackState::Playing) {
            emit positionChanged(engine.position());
        }
    });
}

void MediaManager::onEngineStateChanged(PlaybackState state)
{
    emit stateChanged(state);

    // Handle automatic playlist advancement
    if (state == PlaybackState::Stopped && m_autoPlay) {
        std::lock_guard<std::mutex> lock(m_playlistMutex);
        if (hasNext()) {
            next();
            play();
        } else if (m_repeatMode && !m_playlist.isEmpty()) {
            setCurrentIndex(0);
            play();
        }
    }
}

void MediaManager::onEnginePositionChanged(qint64 position)
{
    emit positionChanged(position);
}

void MediaManager::onEngineDurationChanged(qint64 duration)
{
    emit durationChanged(duration);
}

void MediaManager::onEngineVolumeChanged(int volume)
{
    emit volumeChanged(volume);
}

void MediaManager::onEngineMutedChanged(bool muted)
{
    emit mutedChanged(muted);
}

void MediaManager::onEnginePlaybackRateChanged(qreal rate)
{
    emit playbackRateChanged(rate);
}

void MediaManager::onEngineMediaInfoChanged()
{
    emit mediaInfoChanged();
}

void MediaManager::onEngineErrorOccurred(const QString& error)
{
    qWarning() << "Media engine error:" << error;
    emit errorOccurred(error);

    // Try to continue with next media if auto-play is enabled
    if (m_autoPlay && hasNext()) {
        next();
    }
}

void MediaManager::connectEngineSignals()
{
    if (!m_engine) return;

    connect(m_engine.get(), &IMediaEngine::stateChanged,
            this, &MediaManager::onEngineStateChanged);
    connect(m_engine.get(), &IMediaEngine::positionChanged,
            this, &MediaManager::onEnginePositionChanged);
    connect(m_engine.get(), &IMediaEngine::durationChanged,
            this, &MediaManager::onEngineDurationChanged);
    connect(m_engine.get(), &IMediaEngine::volumeChanged,
            this, &MediaManager::onEngineVolumeChanged);
    connect(m_engine.get(), &IMediaEngine::mutedChanged,
            this, &MediaManager::onEngineMutedChanged);
    connect(m_engine.get(), &IMediaEngine::playbackRateChanged,
            this, &MediaManager::onEnginePlaybackRateChanged);
    connect(m_engine.get(), &IMediaEngine::mediaInfoChanged,
            this, &MediaManager::onEngineMediaInfoChanged);
    connect(m_engine.get(), &IMediaEngine::errorOccurred,
            this, &MediaManager::onEngineErrorOccurred);
}

void MediaManager::disconnectEngineSignals()
{
    if (!m_engine) return;
    m_engine->disconnect(this);
}

void MediaManager::loadCurrentMedia()
{
    std::lock_guard<std::mutex> lock(m_playlistMutex);
    if (isValidIndex(m_currentIndex)) {
        const QString& url = m_playlist.at(m_currentIndex);
        loadMedia(QUrl(url));
    }
}

bool MediaManager::isValidIndex(int index) const
{
    return index >= 0 && index < m_playlist.size();
}

// Template implementations
template<typename Func>
auto MediaManager::safeEngineCall(Func&& func) const -> decltype(func(std::declval<IMediaEngine&>()))
{
    std::lock_guard<std::recursive_mutex> lock(m_engineMutex);

    using ReturnType = decltype(func(std::declval<IMediaEngine&>()));

    if (!m_engine) {
        if constexpr (std::is_same_v<ReturnType, bool>) {
            return false;
        } else if constexpr (std::is_arithmetic_v<ReturnType>) {
            return ReturnType{0};
        } else {
            return ReturnType{};
        }
    }

    try {
        return func(*m_engine);
    } catch (const std::exception& e) {
        qWarning() << "Engine operation failed:" << e.what();
        if constexpr (std::is_same_v<ReturnType, bool>) {
            return false;
        } else if constexpr (std::is_arithmetic_v<ReturnType>) {
            return ReturnType{0};
        } else {
            return ReturnType{};
        }
    }
}

template<typename Func>
bool MediaManager::safeEngineCallVoid(Func&& func) const
{
    std::lock_guard<std::recursive_mutex> lock(m_engineMutex);

    if (!m_engine) {
        return false;
    }

    try {
        func(*m_engine);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Engine operation failed:" << e.what();
        return false;
    }
}

} // namespace DarkPlay::Media
