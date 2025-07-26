#ifndef DARKPLAY_MEDIA_QTMEDIAENGINE_H
#define DARKPLAY_MEDIA_QTMEDIAENGINE_H

#include "IMediaEngine.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <memory>

// Forward declaration to avoid unused include warning
class QVideoSink;

namespace DarkPlay::Media
{

    /**
     * @brief Qt Multimedia implementation of IMediaEngine
     */
    class QtMediaEngine : public IMediaEngine
    {
        Q_OBJECT

    public:
        explicit QtMediaEngine(QObject* parent = nullptr);
        ~QtMediaEngine() override;

        // IMediaEngine implementation
        bool loadMedia(const QUrl& url) override;
        void play() override;
        void pause() override;
        void stop() override;

        [[nodiscard]] qint64 position() const override;
        [[nodiscard]] qint64 duration() const override;
        void setPosition(qint64 position) override;

        [[nodiscard]] int volume() const override;
        void setVolume(int volume) override;
        [[nodiscard]] bool isMuted() const override;
        void setMuted(bool muted) override;

        [[nodiscard]] qreal playbackRate() const override;
        void setPlaybackRate(qreal rate) override;

        [[nodiscard]] PlaybackState state() const override;
        [[nodiscard]] MediaType mediaType() const override;
        [[nodiscard]] QString errorString() const override;

        [[nodiscard]] QString title() const override;
        [[nodiscard]] QSize videoSize() const override;
        [[nodiscard]] bool hasVideo() const override;
        [[nodiscard]] bool hasAudio() const override;

        // Video output support
        void setVideoSink(QVideoSink* sink);
        [[nodiscard]] QVideoSink* videoSink() const;

    private slots:
        void onPlayerStateChanged(QMediaPlayer::PlaybackState state);
        void onPlayerMediaStatusChanged(QMediaPlayer::MediaStatus status);
        void onPlayerErrorOccurred(QMediaPlayer::Error error, const QString& errorString);
        void onPlayerPositionChanged(qint64 position);
        void onPlayerDurationChanged(qint64 duration);
        void onAudioOutputVolumeChanged(float volume);
        void onAudioOutputMutedChanged(bool muted);

    private:
        [[nodiscard]] PlaybackState convertState(QMediaPlayer::PlaybackState qtState) const;
        [[nodiscard]] MediaType detectMediaType(const QUrl& url) const;
        void updateVideoInfo();
        void initializeAudioOutput();
        [[nodiscard]] float getSystemVolumeLevel() const;

        std::unique_ptr<QMediaPlayer> m_player;
        std::unique_ptr<QAudioOutput> m_audioOutput;
        QVideoSink* m_videoSink; // Not owned
        QString m_lastError;
        QSize m_videoSize;
        MediaType m_currentMediaType;
    };

} // namespace DarkPlay::Media

#endif // DARKPLAY_MEDIA_QTMEDIAENGINE_H
