#ifndef DARKPLAY_CORE_PLUGINMANAGER_H
#define DARKPLAY_CORE_PLUGINMANAGER_H

#include <QObject>
#include <QPluginLoader>
#include <QStringList>
#include <QMutex>
#include <QReadWriteLock>
#include <memory>
#include <unordered_map>
#include <atomic>


namespace DarkPlay::Plugins { class IPlugin; }


namespace DarkPlay::Core {

/**
 * @brief Thread-safe manager for plugin loading, unloading, and lifecycle
 * Provides RAII-compliant plugin management with proper error handling
 */
class PluginManager : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject* parent = nullptr);
    ~PluginManager() override;

    // Delete copy and move operations for safety
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager& operator=(PluginManager&&) = delete;

    // Plugin management - thread-safe with proper error handling
    bool loadPlugin(const QString& filePath) noexcept;
    bool unloadPlugin(const QString& pluginName) noexcept;
    void loadAllPlugins(const QString& pluginsDirectory = "plugins") noexcept;
    void unloadAllPlugins() noexcept;

    // Plugin access - thread-safe
    [[nodiscard]] QStringList availablePlugins() const;
    [[nodiscard]] Plugins::IPlugin* getPlugin(const QString& name) const;
    
    template<typename T>
    [[nodiscard]] QList<T*> getPluginsOfType() const;

    // Plugin state queries
    [[nodiscard]] bool isPluginLoaded(const QString& name) const noexcept;
    [[nodiscard]] bool isPluginEnabled(const QString& name) const noexcept;
    
    // Plugin control
    bool enablePlugin(const QString& name) noexcept;
    bool disablePlugin(const QString& name) noexcept;

    // Statistics
    [[nodiscard]] int loadedPluginCount() const noexcept;
    [[nodiscard]] int enabledPluginCount() const noexcept;

signals:
    void pluginLoaded(const QString& name);
    void pluginUnloaded(const QString& name);
    void pluginEnabled(const QString& name);
    void pluginDisabled(const QString& name);
    void pluginError(const QString& name, const QString& error);

private:
    struct PluginInfo {
        QObject* pluginObject{nullptr};           // Raw pointer managed by QPluginLoader
        std::unique_ptr<QPluginLoader> loader;    // RAII for QPluginLoader
        QString filePath;
        std::atomic<bool> enabled{false};
        
        // Custom constructors to handle atomic member
        PluginInfo() = default;

        PluginInfo(const PluginInfo&) = delete;
        PluginInfo& operator=(const PluginInfo&) = delete;

        // Custom move constructor
        PluginInfo(PluginInfo&& other) noexcept
            : pluginObject(other.pluginObject)
            , loader(std::move(other.loader))
            , filePath(std::move(other.filePath))
            , enabled(other.enabled.load())
        {
            other.pluginObject = nullptr;
            other.enabled.store(false);
        }

        // Custom move assignment
        PluginInfo& operator=(PluginInfo&& other) noexcept
        {
            if (this != &other) {
                pluginObject = other.pluginObject;
                loader = std::move(other.loader);
                filePath = std::move(other.filePath);
                enabled.store(other.enabled.load());

                other.pluginObject = nullptr;
                other.enabled.store(false);
            }
            return *this;
        }
    };

    // Thread-safe container access
    mutable QReadWriteLock m_pluginsLock;
    std::unordered_map<QString, PluginInfo> m_loadedPlugins;
    QString m_pluginsDirectory;

    // Helper methods
    [[nodiscard]] static bool validatePlugin(const Plugins::IPlugin* plugin) noexcept;
    void initializePlugin(Plugins::IPlugin* plugin);
    void emitError(const QString& pluginName, const QString& error) const noexcept;
    
    // Safe plugin access
    [[nodiscard]] Plugins::IPlugin* getPluginUnsafe(const QString& name) const noexcept;
};

template<typename T>
QList<T*> PluginManager::getPluginsOfType() const
{
    QReadLocker locker(&m_pluginsLock);
    
    QList<T*> result;
    for (const auto& [name, info] : m_loadedPlugins) {
        if (info.enabled.load(std::memory_order_acquire)) {
            if (auto* plugin = qobject_cast<T*>(info.pluginObject)) {
                result.append(plugin);
            }
        }
    }
    return result;
}

} // namespace DarkPlay::Core

#endif // DARKPLAY_CORE_PLUGINMANAGER_H
