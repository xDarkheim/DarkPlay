#include "media/QtMediaEngine.h"
#include <QFileInfo>
#include <QDebug>
#include <QVideoSink>
#include <QMediaMetaData>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QProcess>
#include <QRegularExpression>

namespace DarkPlay::Media {

QtMediaEngine::QtMediaEngine(QObject* parent)
    : IMediaEngine(parent)
    , m_player(std::make_unique<QMediaPlayer>(this))
    , m_audioOutput(std::make_unique<QAudioOutput>(this))
    , m_videoSink(nullptr)
    , m_currentMediaType(MediaType::Unknown)
{
    // Initialize audio output with proper settings
    initializeAudioOutput();

    // Set up audio output
    m_player->setAudioOutput(m_audioOutput.get());

    // Connect signals
    connect(m_player.get(), &QMediaPlayer::playbackStateChanged,
            this, &QtMediaEngine::onPlayerStateChanged);
    connect(m_player.get(), &QMediaPlayer::mediaStatusChanged,
            this, &QtMediaEngine::onPlayerMediaStatusChanged);
    connect(m_player.get(), &QMediaPlayer::errorOccurred,
            this, &QtMediaEngine::onPlayerErrorOccurred);
    connect(m_player.get(), &QMediaPlayer::positionChanged,
            this, &QtMediaEngine::onPlayerPositionChanged);
    connect(m_player.get(), &QMediaPlayer::durationChanged,
            this, &QtMediaEngine::onPlayerDurationChanged);

    // Audio output signals
    connect(m_audioOutput.get(), &QAudioOutput::volumeChanged,
            this, &QtMediaEngine::onAudioOutputVolumeChanged);
    connect(m_audioOutput.get(), &QAudioOutput::mutedChanged,
            this, &QtMediaEngine::onAudioOutputMutedChanged);

    qDebug() << "QtMediaEngine initialized";
}

QtMediaEngine::~QtMediaEngine() = default;

bool QtMediaEngine::loadMedia(const QUrl& url)
{
    if (!url.isValid()) {
        m_lastError = "Invalid URL provided";
        emit errorOccurred(m_lastError);
        return false;
    }

    m_lastError.clear();

    // Stop current playback if any
    if (m_player->playbackState() != QMediaPlayer::StoppedState) {
        m_player->stop();
    }

    // Reset position to beginning for new media
    m_player->setPosition(0);

    // Clear previous video info
    m_videoSize = QSize();

    m_currentMediaType = detectMediaType(url);
    m_player->setSource(url);

    // Emit signals to reset UI state
    emit positionChanged(0);
    emit durationChanged(0);

    return true;
}

void QtMediaEngine::play()
{
    if (m_player->source().isEmpty()) {
        m_lastError = "No media loaded";
        emit errorOccurred(m_lastError);
        return;
    }

    // FIX: If media has ended (position at or near the end), reset to beginning
    qint64 currentPos = m_player->position();
    qint64 duration = m_player->duration();

    if (duration > 0 && currentPos >= duration - 100) { // 100ms tolerance
        m_player->setPosition(0);
    }

    m_player->play();
}

void QtMediaEngine::pause()
{
    m_player->pause();
}

void QtMediaEngine::stop()
{
    m_player->stop();
}

qint64 QtMediaEngine::position() const
{
    return m_player->position();
}

qint64 QtMediaEngine::duration() const
{
    return m_player->duration();
}

void QtMediaEngine::setPosition(qint64 position)
{
    m_player->setPosition(position);
}

int QtMediaEngine::volume() const
{
    // Convert from 0.0-1.0 to 0-100
    return static_cast<int>(m_audioOutput->volume() * 100);
}

void QtMediaEngine::setVolume(int volume)
{
    // Convert from 0-100 to 0.0-1.0
    float normalizedVolume = qBound(0.0f, volume / 100.0f, 1.0f);
    m_audioOutput->setVolume(normalizedVolume);
}

bool QtMediaEngine::isMuted() const
{
    return m_audioOutput->isMuted();
}

void QtMediaEngine::setMuted(bool muted)
{
    m_audioOutput->setMuted(muted);
}

qreal QtMediaEngine::playbackRate() const
{
    return m_player->playbackRate();
}

void QtMediaEngine::setPlaybackRate(qreal rate)
{
    m_player->setPlaybackRate(rate);
}

PlaybackState QtMediaEngine::state() const
{
    return convertState(m_player->playbackState());
}

MediaType QtMediaEngine::mediaType() const
{
    return m_currentMediaType;
}

QString QtMediaEngine::errorString() const
{
    return m_lastError;
}

QString QtMediaEngine::title() const
{
    // Try to get title from metadata, fallback to filename
    auto metaData = m_player->metaData();
    if (metaData.stringValue(QMediaMetaData::Title).isEmpty() == false) {
        return metaData.stringValue(QMediaMetaData::Title);
    }

    // Fallback to filename
    QUrl source = m_player->source();
    if (source.isLocalFile()) {
        QFileInfo fileInfo(source.toLocalFile());
        return fileInfo.baseName();
    }

    return source.toString();
}

QSize QtMediaEngine::videoSize() const
{
    return m_videoSize;
}

bool QtMediaEngine::hasVideo() const
{
    return m_currentMediaType == MediaType::Video || m_player->hasVideo();
}

bool QtMediaEngine::hasAudio() const
{
    return m_player->hasAudio();
}

void QtMediaEngine::setVideoSink(QVideoSink* sink)
{
    m_videoSink = sink;
    m_player->setVideoSink(sink);
}

QVideoSink* QtMediaEngine::videoSink() const
{
    return m_videoSink;
}

// Private slots
void QtMediaEngine::onPlayerStateChanged(QMediaPlayer::PlaybackState state)
{
    emit stateChanged(convertState(state));
}

void QtMediaEngine::onPlayerMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::LoadedMedia:
        updateVideoInfo();
        emit mediaLoaded();
        break;
    case QMediaPlayer::BufferingMedia:
        emit stateChanged(PlaybackState::Buffering);
        break;
    case QMediaPlayer::EndOfMedia:
        // When media reaches the end, emit stopped state
        emit stateChanged(PlaybackState::Stopped);
        break;
    case QMediaPlayer::InvalidMedia:
        m_lastError = "Invalid media format";
        emit errorOccurred(m_lastError);
        break;
    default:
        break;
    }
}

void QtMediaEngine::onPlayerErrorOccurred(QMediaPlayer::Error error, const QString& errorString)
{
    Q_UNUSED(error)

    // Filter out common non-critical warnings
    if (errorString.contains("env_facs_q") ||
        errorString.contains("AAC") ||
        errorString.toLower().contains("warning")) {
        qDebug() << "Non-critical media warning:" << errorString;
        return;
    }

    m_lastError = errorString.isEmpty() ? "Unknown media error" : errorString;
    emit errorOccurred(m_lastError);
}

void QtMediaEngine::onPlayerPositionChanged(qint64 position)
{
    emit positionChanged(position);
}

void QtMediaEngine::onPlayerDurationChanged(qint64 duration)
{
    emit durationChanged(duration);
}

void QtMediaEngine::onAudioOutputVolumeChanged(float volume)
{
    // Convert from 0.0-1.0 to 0-100
    emit volumeChanged(static_cast<int>(volume * 100));
}

void QtMediaEngine::onAudioOutputMutedChanged(bool muted)
{
    emit mutedChanged(muted);
}

// Private methods
PlaybackState QtMediaEngine::convertState(QMediaPlayer::PlaybackState qtState) const
{
    switch (qtState) {
    case QMediaPlayer::StoppedState:
        return PlaybackState::Stopped;
    case QMediaPlayer::PlayingState:
        return PlaybackState::Playing;
    case QMediaPlayer::PausedState:
        return PlaybackState::Paused;
    }
    return PlaybackState::Stopped;
}

MediaType QtMediaEngine::detectMediaType(const QUrl& url) const
{
    if (url.isLocalFile()) {
        QFileInfo fileInfo(url.toLocalFile());
        QString suffix = fileInfo.suffix().toLower();

        QStringList videoExtensions = {"mp4", "avi", "mkv", "mov", "wmv", "flv", "webm", "m4v"};
        QStringList audioExtensions = {"mp3", "wav", "flac", "ogg", "aac", "m4a", "wma"};

        if (videoExtensions.contains(suffix)) {
            return MediaType::Video;
        } else if (audioExtensions.contains(suffix)) {
            return MediaType::Audio;
        }
    }

    return MediaType::Unknown;
}

void QtMediaEngine::updateVideoInfo()
{
    // Get video resolution from metadata
    auto metaData = m_player->metaData();
    QSize resolution = metaData.value(QMediaMetaData::Resolution).toSize();
    if (resolution.isValid()) {
        m_videoSize = resolution;
    } else {
        // Fallback to default size if no metadata available
        m_videoSize = QSize();
    }
}

void QtMediaEngine::initializeAudioOutput()
{
    // Get the default audio output device
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();

    if (defaultDevice.isNull()) {
        qWarning() << "No default audio output device found";
        return;
    }

    // Set the audio device for the output
    m_audioOutput->setDevice(defaultDevice);

    // NEW APPROACH: Always use high volume for media player, ignore system volume
    // Media players should be loud and user can adjust if needed
    float mediaVolume = 0.95f; // Start with 95% - very loud but safe

    // Try to get system volume only for logging purposes
    float systemVolume = getSystemVolumeLevel();
    if (systemVolume > 0.0f) {
        qDebug() << "System volume detected:" << systemVolume << "but using media volume:" << mediaVolume;
    } else {
        qDebug() << "System volume unavailable, using high media volume:" << mediaVolume;
    }

    // Set high volume for media playback
    m_audioOutput->setVolume(mediaVolume);

    // Ensure audio is not muted
    m_audioOutput->setMuted(false);

    qDebug() << "Audio output initialized with device:" << defaultDevice.description();
    qDebug() << "Media volume set to:" << mediaVolume << "(95%)";
    qDebug() << "Final audio output volume:" << m_audioOutput->volume();
    qDebug() << "Audio output muted:" << m_audioOutput->isMuted();

    // List available audio devices for debugging
    auto audioDevices = QMediaDevices::audioOutputs();
    qDebug() << "Available audio output devices:";
    for (const auto& device : audioDevices) {
        qDebug() << "  -" << device.description() << (device.isDefault() ? "(default)" : "");
    }
}

float QtMediaEngine::getSystemVolumeLevel() const
{
    // Try to get system volume from PulseAudio (common on Linux)
    QProcess process;
    process.start("pactl", QStringList() << "get-sink-volume" << "@DEFAULT_SINK@");
    process.waitForFinished(1000);

    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        QRegularExpression re(R"((\d+)%)");
        QRegularExpressionMatch match = re.match(output);

        if (match.hasMatch()) {
            int volumePercent = match.captured(1).toInt();
            float volume = volumePercent / 100.0f;
            qDebug() << "System volume detected:" << volumePercent << "% (" << volume << ")";
            return qBound(0.0f, volume, 1.0f);
        }
    }

    // Fallback: try ALSA mixer
    process.start("amixer", QStringList() << "get" << "Master");
    process.waitForFinished(1000);

    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        QRegularExpression re(R"(\[(\d+)%\])");
        QRegularExpressionMatch match = re.match(output);

        if (match.hasMatch()) {
            int volumePercent = match.captured(1).toInt();
            float volume = volumePercent / 100.0f;
            qDebug() << "ALSA system volume detected:" << volumePercent << "% (" << volume << ")";
            return qBound(0.0f, volume, 1.0f);
        }
    }

    qDebug() << "Could not detect system volume, using default";
    return -1.0f; // Signal that system volume is unavailable
}
} // namespace DarkPlay::Media

