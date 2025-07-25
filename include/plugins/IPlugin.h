#ifndef DARKPLAY_PLUGINS_IPLUGIN_H
#define DARKPLAY_PLUGINS_IPLUGIN_H

#include <QObject>
#include <QString>
#include <QJsonObject>

namespace DarkPlay {
namespace Plugins {

/**
 * @brief Base interface for all plugins
 */
class IPlugin : public QObject
{
    Q_OBJECT

public:
    ~IPlugin() override = default;

    // Plugin metadata
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString description() const = 0;
    virtual QStringList dependencies() const { return {}; }

    // Plugin lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isEnabled() const = 0;

    // Configuration
    virtual QJsonObject defaultConfig() const { return {}; }
    virtual void applyConfig(const QJsonObject& config) { Q_UNUSED(config) }

signals:
    void statusChanged(bool enabled);
    void errorOccurred(const QString& error);
};

/**
 * @brief Interface for media codec plugins
 */
class IMediaCodecPlugin : public IPlugin
{
    Q_OBJECT

public:
    virtual QStringList supportedFormats() const = 0;
    virtual bool canDecode(const QString& format) const = 0;
    virtual bool canEncode(const QString& format) const = 0;
};

/**
 * @brief Interface for UI theme plugins
 */
class IThemePlugin : public IPlugin
{
    Q_OBJECT

public:
    virtual QString themeName() const = 0;
    virtual QString themeStyleSheet() const = 0;
    virtual QJsonObject themeColors() const = 0;
};

/**
 * @brief Interface for audio effect plugins
 */
class IAudioEffectPlugin : public IPlugin
{
    Q_OBJECT

public:
    virtual void processAudio(float* buffer, int samples, int channels) = 0;
    virtual QWidget* createControlWidget() = 0;
};

} // namespace Plugins
} // namespace DarkPlay

Q_DECLARE_INTERFACE(DarkPlay::Plugins::IPlugin, "com.darkplay.IPlugin/1.0")
Q_DECLARE_INTERFACE(DarkPlay::Plugins::IMediaCodecPlugin, "com.darkplay.IMediaCodecPlugin/1.0")
Q_DECLARE_INTERFACE(DarkPlay::Plugins::IThemePlugin, "com.darkplay.IThemePlugin/1.0")
Q_DECLARE_INTERFACE(DarkPlay::Plugins::IAudioEffectPlugin, "com.darkplay.IAudioEffectPlugin/1.0")

#endif // DARKPLAY_PLUGINS_IPLUGIN_H
