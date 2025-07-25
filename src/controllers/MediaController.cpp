#include "controllers/MediaController.h"
#include "media/MediaManager.h"
#include "media/IMediaEngine.h"
#include <QFileInfo>
#include <QUrl>
#include <QVideoSink>
#include <QDebug>

namespace DarkPlay::Controllers {

MediaController::MediaController(QObject* parent)
    : QObject(parent)
    , m_mediaManager(std::make_unique<Media::MediaManager>(this))
    , m_currentVolume(0.7f)
{
    try {
        // Create default media engine with proper error handling
        m_currentEngine.reset(m_mediaManager->createEngine());

        if (m_currentEngine) {
            connectEngineSignals();
            // Set initial volume
            m_currentEngine->setVolume(m_currentVolume);
        } else {
            qWarning() << "Failed to create media engine";
        }
    } catch (const std::exception& e) {
        qCritical() << "Exception in MediaController constructor:" << e.what();
        // Don't rethrow - let object be constructed but mark as invalid
        m_currentEngine.reset();
    }
}

bool MediaController::loadMedia(const QString& filePath) noexcept
{
    try {
        if (!m_currentEngine) {
            emit errorOccurred("No media engine available");
            return false;
        }

        if (filePath.isEmpty()) {
            emit errorOccurred("Empty file path provided");
            return false;
        }

        const QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            emit errorOccurred(QString("File does not exist: %1").arg(filePath));
            return false;
        }

        if (!fileInfo.isReadable()) {
            emit errorOccurred(QString("File is not readable: %1").arg(filePath));
            return false;
        }

        // Create URL and validate
        const QUrl mediaUrl = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
        if (!mediaUrl.isValid()) {
            emit errorOccurred(QString("Invalid file path: %1").arg(filePath));
            return false;
        }

        // Check if engine can handle this file
        if (!m_currentEngine->canPlay(mediaUrl)) {
            emit errorOccurred(QString("Unsupported media format: %1").arg(fileInfo.suffix()));
            return false;
        }

        // Set source with error handling
        m_currentEngine->setSource(mediaUrl);
        m_currentFilePath = filePath;

        emit mediaLoaded(fileInfo.fileName());
        return true;

    } catch (const std::exception& e) {
        const QString error = QString("Failed to load media: %1").arg(e.what());
        emit errorOccurred(error);
        return false;
    } catch (...) {
        emit errorOccurred("Unknown error occurred while loading media");
        return false;
    }
}

void MediaController::play() noexcept
{
    try {
        if (m_currentEngine && hasMedia()) {
            m_currentEngine->play();
        } else if (!hasMedia()) {
            emit errorOccurred("No media loaded");
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString("Playback error: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown playback error");
    }
}

void MediaController::pause() noexcept
{
    try {
        if (m_currentEngine) {
            m_currentEngine->pause();
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString("Pause error: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown pause error");
    }
}

void MediaController::stop() noexcept
{
    try {
        if (m_currentEngine) {
            m_currentEngine->stop();
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString("Stop error: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown stop error");
    }
}

void MediaController::seek(qint64 position) noexcept
{
    try {
        if (m_currentEngine && hasMedia()) {
            const qint64 clampedPosition = qBound(0LL, position, duration());
            m_currentEngine->setPosition(clampedPosition);
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString("Seek error: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown seek error");
    }
}

void MediaController::setVolume(float volume) noexcept
{
    try {
        // Clamp volume to valid range
        const float clampedVolume = qBound(0.0f, volume, 1.0f);
        m_currentVolume = clampedVolume;

        if (m_currentEngine) {
            m_currentEngine->setVolume(clampedVolume);
        }

        emit volumeChanged(clampedVolume);
    } catch (const std::exception& e) {
        emit errorOccurred(QString("Volume control error: %1").arg(e.what()));
    } catch (...) {
        emit errorOccurred("Unknown volume control error");
    }
}

void MediaController::setVideoSink(QVideoSink* sink) noexcept
{
    try {
        m_videoSink = sink; // QPointer handles null automatically

        if (m_currentEngine) {
            m_currentEngine->setVideoSink(sink);
        }
    } catch (const std::exception& e) {
        qWarning() << "Error setting video sink:" << e.what();
    } catch (...) {
        qWarning() << "Unknown error setting video sink";
    }
}

QVideoSink* MediaController::videoSink() const noexcept
{
    return m_videoSink.data(); // Safe even if null
}

bool MediaController::hasMedia() const noexcept
{
    try {
        return m_currentEngine &&
               !m_currentEngine->source().isEmpty() &&
               m_currentEngine->source().isValid();
    } catch (...) {
        return false;
    }
}

qint64 MediaController::position() const noexcept
{
    try {
        return m_currentEngine ? m_currentEngine->position() : 0;
    } catch (...) {
        return 0;
    }
}

qint64 MediaController::duration() const noexcept
{
    try {
        return m_currentEngine ? m_currentEngine->duration() : 0;
    } catch (...) {
        return 0;
    }
}

float MediaController::volume() const noexcept
{
    return m_currentVolume; // Use cached value to avoid exceptions
}

Media::IMediaEngine::State MediaController::state() const noexcept
{
    try {
        return m_currentEngine ? m_currentEngine->state() : Media::IMediaEngine::State::Stopped;
    } catch (...) {
        return Media::IMediaEngine::State::Error;
    }
}

void MediaController::connectEngineSignals() noexcept
{
    if (!m_currentEngine) {
        return;
    }

    try {
        connect(m_currentEngine.get(), &Media::IMediaEngine::positionChanged,
                this, &MediaController::onEnginePositionChanged);
        connect(m_currentEngine.get(), &Media::IMediaEngine::durationChanged,
                this, &MediaController::onEngineDurationChanged);
        connect(m_currentEngine.get(), &Media::IMediaEngine::stateChanged,
                this, &MediaController::onEngineStateChanged);
        connect(m_currentEngine.get(), &Media::IMediaEngine::errorOccurred,
                this, &MediaController::onEngineError);
    } catch (const std::exception& e) {
        qWarning() << "Error connecting engine signals:" << e.what();
    } catch (...) {
        qWarning() << "Unknown error connecting engine signals";
    }
}

void MediaController::disconnectEngineSignals() noexcept
{
    if (!m_currentEngine) {
        return;
    }

    try {
        disconnect(m_currentEngine.get(), nullptr, this, nullptr);
    } catch (const std::exception& e) {
        qWarning() << "Error disconnecting engine signals:" << e.what();
    } catch (...) {
        qWarning() << "Unknown error disconnecting engine signals";
    }
}

void MediaController::onEnginePositionChanged(qint64 position)
{
    emit positionChanged(position);
}

void MediaController::onEngineDurationChanged(qint64 duration)
{
    emit durationChanged(duration);
}

void MediaController::onEngineStateChanged(Media::IMediaEngine::State state)
{
    emit stateChanged(state);
}

void MediaController::onEngineError(const QString& error)
{
    emit errorOccurred(error);
}

} // namespace DarkPlay::Controllers
