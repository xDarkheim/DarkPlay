#ifndef DARKPLAY_MEDIA_MEDIAMANAGER_H
#define DARKPLAY_MEDIA_MEDIAMANAGER_H

#include <memory>
#include "IMediaEngine.h"

namespace DarkPlay {
namespace Media {

class IMediaEngineFactory;

/**
 * @brief Central manager for media playback functionality
 */
class MediaManager : public QObject
{
    Q_OBJECT

public:
    explicit MediaManager(QObject* parent = nullptr);
    ~MediaManager() override;

    // Engine management
    void registerEngineFactory(std::unique_ptr<IMediaEngineFactory> factory);
    QStringList availableEngines() const;
    IMediaEngine* createEngine(const QString& engineName = QString()) const;
    IMediaEngine* createEngineForUrl(const QUrl& url) const;

    // Current playback
    IMediaEngine* currentEngine() const { return m_currentEngine.get(); }
    void setCurrentEngine(std::unique_ptr<IMediaEngine> engine);

    // Media information
    QStringList supportedFormats() const;
    bool canPlay(const QUrl& url) const;

    // Playlist management (future extension)
    // void setPlaylist(IPlaylist* playlist);
    // IPlaylist* playlist() const;

signals:
    void engineChanged(IMediaEngine* engine);
    void supportedFormatsChanged();

private:
    std::vector<std::unique_ptr<IMediaEngineFactory>> m_engineFactories;
    std::unique_ptr<IMediaEngine> m_currentEngine;

    void setupDefaultEngines();
    IMediaEngineFactory* findBestEngine(const QUrl& url) const;
};

} // namespace Media
} // namespace DarkPlay

#endif // DARKPLAY_MEDIA_MEDIAMANAGER_H
