#include "controllers/MediaController.h"
#include "media/QtMediaEngine.h"
#include "media/MediaManager.h"
#include <QFileInfo>
#include <QDebug>

namespace DarkPlay::Controllers {

MediaController::MediaController(QObject* parent)
    : QObject(parent)
    , m_mediaManager(std::make_unique<Media::MediaManager>(this))
{
    setupConnections();
    initializeDefaultEngine();
}

MediaController::~MediaController() = default;

bool MediaController::openFile(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        m_lastError = QString("File does not exist or is not readable: %1").arg(filePath);
        emit mediaLoadFailed(m_lastError);
        return false;
    }

    QUrl fileUrl = QUrl::fromLocalFile(filePath);
    return openUrl(fileUrl);
}

bool MediaController::openUrl(const QUrl& url)
{
    if (!url.isValid()) {
        m_lastError = "Invalid URL provided";
        emit mediaLoadFailed(m_lastError);
        return false;
    }

    bool success = m_mediaManager->loadMedia(url);
    if (success) {
        emit mediaOpened(url.toString());
    } else {
        m_lastError = m_mediaManager->errorString();
        if (m_lastError.isEmpty()) {
            m_lastError = "Failed to load media";
        }
        emit mediaLoadFailed(m_lastError);
    }

    return success;
}

void MediaController::play()
{
    m_mediaManager->play();
}

void MediaController::pause()
{
    m_mediaManager->pause();
}

void MediaController::stop()
{
    m_mediaManager->stop();
}

void MediaController::togglePlayPause()
{
    m_mediaManager->togglePlayPause();
}

void MediaController::seek(qint64 position)
{
    m_mediaManager->setPosition(position);
}

void MediaController::seekRelative(qint64 offset)
{
    m_mediaManager->seek(offset);
}

qint64 MediaController::position() const
{
    return m_mediaManager->position();
}

qint64 MediaController::duration() const
{
    return m_mediaManager->duration();
}

void MediaController::setVolume(int volume)
{
    m_mediaManager->setVolume(volume);
}

int MediaController::volume() const
{
    return m_mediaManager->volume();
}

void MediaController::setMuted(bool muted)
{
    m_mediaManager->setMuted(muted);
}

bool MediaController::isMuted() const
{
    return m_mediaManager->isMuted();
}

void MediaController::setPlaybackRate(qreal rate)
{
    m_mediaManager->setPlaybackRate(rate);
}

qreal MediaController::playbackRate() const
{
    return m_mediaManager->playbackRate();
}

Media::PlaybackState MediaController::state() const
{
    return m_mediaManager->state();
}

QString MediaController::errorString() const
{
    return m_lastError.isEmpty() ? m_mediaManager->errorString() : m_lastError;
}

bool MediaController::hasMedia() const
{
    return !m_mediaManager->currentMediaUrl().isEmpty();
}

QString MediaController::currentMediaUrl() const
{
    return m_mediaManager->currentMediaUrl();
}

QString MediaController::title() const
{
    return m_mediaManager->title();
}

QSize MediaController::videoSize() const
{
    return m_mediaManager->videoSize();
}

bool MediaController::hasVideo() const
{
    return m_mediaManager->hasVideo();
}

bool MediaController::hasAudio() const
{
    return m_mediaManager->hasAudio();
}

// Public slots
void MediaController::onPlayRequested()
{
    play();
}

void MediaController::onPauseRequested()
{
    pause();
}

void MediaController::onStopRequested()
{
    stop();
}

void MediaController::onVolumeChangeRequested(int volume)
{
    setVolume(volume);
}

void MediaController::onSeekRequested(qint64 position)
{
    seek(position);
}

// Private slots - MediaManager signal handlers
void MediaController::onManagerStateChanged(Media::PlaybackState state)
{
    emit playbackStateChanged(state);
    emit stateChanged(state);  // Emit both signals for compatibility
}

void MediaController::onManagerPositionChanged(qint64 position)
{
    emit positionChanged(position);
}

void MediaController::onManagerDurationChanged(qint64 duration)
{
    emit durationChanged(duration);
}

void MediaController::onManagerVolumeChanged(int volume)
{
    emit volumeChanged(volume);
}

void MediaController::onManagerMutedChanged(bool muted)
{
    emit mutedChanged(muted);
}

void MediaController::onManagerPlaybackRateChanged(qreal rate)
{
    emit playbackRateChanged(rate);
}

void MediaController::onManagerMediaLoaded(const QString& url)
{
    m_lastError.clear();
    emit mediaInfoChanged();
    emit mediaOpened(url);
}

void MediaController::onManagerError(const QString& error)
{
    m_lastError = error;
    emit errorOccurred(error);
}

void MediaController::setupConnections()
{
    if (!m_mediaManager) return;

    connect(m_mediaManager.get(), &Media::MediaManager::stateChanged,
            this, &MediaController::onManagerStateChanged);
    connect(m_mediaManager.get(), &Media::MediaManager::positionChanged,
            this, &MediaController::onManagerPositionChanged);
    connect(m_mediaManager.get(), &Media::MediaManager::durationChanged,
            this, &MediaController::onManagerDurationChanged);
    connect(m_mediaManager.get(), &Media::MediaManager::volumeChanged,
            this, &MediaController::onManagerVolumeChanged);
    connect(m_mediaManager.get(), &Media::MediaManager::mutedChanged,
            this, &MediaController::onManagerMutedChanged);
    connect(m_mediaManager.get(), &Media::MediaManager::playbackRateChanged,
            this, &MediaController::onManagerPlaybackRateChanged);
    connect(m_mediaManager.get(), &Media::MediaManager::mediaLoaded,
            this, &MediaController::onManagerMediaLoaded);
    connect(m_mediaManager.get(), &Media::MediaManager::error,
            this, &MediaController::onManagerError);
}

void MediaController::initializeDefaultEngine()
{
    // Create and set up Qt Media Engine as the default engine
    auto qtEngine = std::make_unique<Media::QtMediaEngine>();

    if (qtEngine) {
        m_mediaManager->setMediaEngine(std::move(qtEngine));
        qDebug() << "MediaController initialized with Qt Media Engine";
    } else {
        qWarning() << "Failed to create Qt Media Engine";
    }
}

void MediaController::setVideoSink(QVideoSink* sink)
{
    if (m_mediaManager && m_mediaManager->mediaEngine()) {
        // Check if the engine supports video output
        if (auto* qtEngine = qobject_cast<Media::QtMediaEngine*>(m_mediaManager->mediaEngine())) {
            qtEngine->setVideoSink(sink);
        }
    }
}

QVideoSink* MediaController::videoSink() const
{
    if (m_mediaManager && m_mediaManager->mediaEngine()) {
        if (auto* qtEngine = qobject_cast<Media::QtMediaEngine*>(m_mediaManager->mediaEngine())) {
            return qtEngine->videoSink();
        }
    }
    return nullptr;
}

} // namespace DarkPlay::Controllers
