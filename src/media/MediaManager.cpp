#include "media/MediaManager.h"
#include <QDebug>

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
    return m_engine.get();
}

bool MediaManager::loadMedia(const QUrl& url)
{
    if (!m_engine) {
        qWarning() << "No media engine available";
        return false;
    }

    m_currentUrl = url.toString();
    bool success = m_engine->loadMedia(url);

    if (success) {
        emit mediaLoaded(m_currentUrl);
    }

    return success;
}

void MediaManager::play()
{
    if (m_engine) {
        m_engine->play();
        if (!m_positionTimer->isActive()) {
            m_positionTimer->start();
        }
    }
}

void MediaManager::pause()
{
    if (m_engine) {
        m_engine->pause();
        m_positionTimer->stop();
    }
}

void MediaManager::stop()
{
    if (m_engine) {
        m_engine->stop();
        m_positionTimer->stop();
    }
}

void MediaManager::togglePlayPause()
{
    if (!m_engine) return;

    PlaybackState currentState = m_engine->state();
    if (currentState == PlaybackState::Playing) {
        pause();
    } else if (currentState == PlaybackState::Paused || currentState == PlaybackState::Stopped) {
        play();
    }
}

qint64 MediaManager::position() const
{
    return m_engine ? m_engine->position() : 0;
}

qint64 MediaManager::duration() const
{
    return m_engine ? m_engine->duration() : 0;
}

void MediaManager::setPosition(qint64 position)
{
    if (m_engine) {
        m_engine->setPosition(position);
    }
}

void MediaManager::seek(qint64 offset)
{
    if (m_engine) {
        qint64 newPosition = position() + offset;
        newPosition = qMax(0LL, qMin(newPosition, duration()));
        setPosition(newPosition);
    }
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
    return m_engine ? m_engine->volume() : 50;
}

void MediaManager::setVolume(int volume)
{
    if (m_engine) {
        volume = qBound(0, volume, 100);
        m_engine->setVolume(volume);
        if (volume > 0) {
            m_previousVolume = volume;
        }
    }
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
    return m_engine ? m_engine->isMuted() : false;
}

void MediaManager::setMuted(bool muted)
{
    if (m_engine) {
        m_engine->setMuted(muted);
    }
}

void MediaManager::toggleMute()
{
    if (!m_engine) return;

    if (isMuted()) {
        setMuted(false);
        if (volume() == 0 && m_previousVolume > 0) {
            setVolume(m_previousVolume);
        }
    } else {
        if (volume() > 0) {
            m_previousVolume = volume();
        }
        setMuted(true);
    }
}

qreal MediaManager::playbackRate() const
{
    return m_engine ? m_engine->playbackRate() : 1.0;
}

void MediaManager::setPlaybackRate(qreal rate)
{
    if (m_engine) {
        rate = qBound(0.25, rate, 4.0); // Limit playback rate
        m_engine->setPlaybackRate(rate);
    }
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
    return m_engine ? m_engine->state() : PlaybackState::Stopped;
}

MediaType MediaManager::mediaType() const
{
    return m_engine ? m_engine->mediaType() : MediaType::Unknown;
}

QString MediaManager::errorString() const
{
    return m_engine ? m_engine->errorString() : QString();
}

QString MediaManager::currentMediaUrl() const
{
    return m_currentUrl;
}

QString MediaManager::title() const
{
    return m_engine ? m_engine->title() : QString();
}

QSize MediaManager::videoSize() const
{
    return m_engine ? m_engine->videoSize() : QSize();
}

bool MediaManager::hasVideo() const
{
    return m_engine ? m_engine->hasVideo() : false;
}

bool MediaManager::hasAudio() const
{
    return m_engine ? m_engine->hasAudio() : false;
}

void MediaManager::setPlaylist(const QStringList& urls)
{
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
    return m_playlist;
}

int MediaManager::currentIndex() const
{
    return m_currentIndex;
}

void MediaManager::setCurrentIndex(int index)
{
    if (isValidIndex(index) && index != m_currentIndex) {
        m_currentIndex = index;
        emit currentIndexChanged(m_currentIndex);
        loadCurrentMedia();
    }
}

void MediaManager::next()
{
    if (hasNext()) {
        setCurrentIndex(m_currentIndex + 1);
    } else if (m_repeatMode && !m_playlist.isEmpty()) {
        setCurrentIndex(0);
    }
}

void MediaManager::previous()
{
    if (hasPrevious()) {
        setCurrentIndex(m_currentIndex - 1);
    } else if (m_repeatMode && !m_playlist.isEmpty()) {
        setCurrentIndex(static_cast<int>(m_playlist.size() - 1));
    }
}

bool MediaManager::hasNext() const
{
    return m_currentIndex < m_playlist.size() - 1;
}

bool MediaManager::hasPrevious() const
{
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
    if (m_engine && m_engine->state() == PlaybackState::Playing) {
        emit positionChanged(m_engine->position());
    }
}

void MediaManager::onEngineStateChanged(PlaybackState state)
{
    emit stateChanged(state);

    // Handle automatic playlist advancement
    if (state == PlaybackState::Stopped && m_autoPlay) {
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

void MediaManager::onEngineError(const QString& error)
{
    qWarning() << "Media engine error:" << error;
    emit this->error(error);

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
            this, &MediaManager::volumeChanged);
    connect(m_engine.get(), &IMediaEngine::mutedChanged,
            this, &MediaManager::mutedChanged);
    connect(m_engine.get(), &IMediaEngine::playbackRateChanged,
            this, &MediaManager::playbackRateChanged);
    connect(m_engine.get(), &IMediaEngine::mediaLoaded,
            this, [this]() { emit mediaLoaded(m_currentUrl); });
    connect(m_engine.get(), &IMediaEngine::error,
            this, &MediaManager::onEngineError);
    connect(m_engine.get(), &IMediaEngine::bufferingProgress,
            this, &MediaManager::bufferingProgress);
}

void MediaManager::disconnectEngineSignals()
{
    if (!m_engine) return;

    m_engine->disconnect(this);
}

void MediaManager::loadCurrentMedia()
{
    if (isValidIndex(m_currentIndex)) {
        const QString& url = m_playlist.at(m_currentIndex);
        loadMedia(QUrl(url));
    }
}

bool MediaManager::isValidIndex(int index) const
{
    return index >= 0 && index < m_playlist.size();
}

} // namespace DarkPlay::Media
