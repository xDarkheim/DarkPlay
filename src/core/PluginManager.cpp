#include "core/PluginManager.h"
#include "plugins/IPlugin.h"
#include <QDir>
#include <QPluginLoader>
#include <QDebug>
#include <QReadLocker>
#include <QWriteLocker>

namespace DarkPlay::Core {

PluginManager::PluginManager(QObject* parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    unloadAllPlugins();
}

bool PluginManager::loadPlugin(const QString& filePath) noexcept
{
    if (filePath.isEmpty()) {
        emitError("", "Empty file path provided");
        return false;
    }

    try {
        QWriteLocker locker(&m_pluginsLock);

        // Create loader with RAII management
        auto loader = std::make_unique<QPluginLoader>(filePath);
        QObject* pluginObject = loader->instance();

        if (!pluginObject) {
            emitError("", QString("Failed to load plugin from %1: %2")
                         .arg(filePath, loader->errorString()));
            return false;
        }

        auto* plugin = qobject_cast<Plugins::IPlugin*>(pluginObject);
        if (!plugin) {
            emitError("", QString("Invalid plugin interface in %1").arg(filePath));
            return false;
        }

        if (!validatePlugin(plugin)) {
            emitError("", QString("Plugin validation failed for %1").arg(filePath));
            return false;
        }

        const QString pluginName = plugin->name();
        if (m_loadedPlugins.contains(pluginName)) {
            emitError(pluginName, "Plugin already loaded");
            return false;
        }

        // Initialize plugin before storing
        // Unlock temporarily for initialization to prevent deadlock
        locker.unlock();

        try {
            initializePlugin(plugin);
        } catch (const std::exception& e) {
            emitError(pluginName, QString("Initialization failed: %1").arg(e.what()));
            return false;
        }

        // Re-lock and store plugin info
        locker.relock();

        PluginInfo info;
        info.pluginObject = pluginObject;  // QPluginLoader manages lifetime
        info.loader = std::move(loader);   // RAII for loader
        info.filePath = filePath;
        info.enabled.store(true, std::memory_order_release);

        m_loadedPlugins.emplace(pluginName, std::move(info));

        // Emit signals outside critical section
        locker.unlock();

        emit pluginLoaded(pluginName);
        emit pluginEnabled(pluginName);

        return true;

    } catch (const std::exception& e) {
        emitError("", QString("Exception during plugin loading: %1").arg(e.what()));
        return false;
    } catch (...) {
        emitError("", "Unknown error during plugin loading");
        return false;
    }
}

bool PluginManager::unloadPlugin(const QString& pluginName) noexcept
{
    if (pluginName.isEmpty()) {
        return false;
    }

    try {
        QWriteLocker locker(&m_pluginsLock);

        auto it = m_loadedPlugins.find(pluginName);
        if (it == m_loadedPlugins.end()) {
            return false;
        }

        // Get plugin reference before removal
        auto* plugin = qobject_cast<Plugins::IPlugin*>(it->second.pluginObject);

        // Perform shutdown outside lock to prevent deadlock
        locker.unlock();

        if (plugin) {
            try {
                plugin->shutdown();
            } catch (const std::exception& e) {
                emitError(pluginName, QString("Shutdown error: %1").arg(e.what()));
            }
        }

        // Re-lock and remove plugin
        locker.relock();

        // Re-find iterator as it may have been invalidated
        it = m_loadedPlugins.find(pluginName);
        if (it != m_loadedPlugins.end()) {
            m_loadedPlugins.erase(it);
        }

        locker.unlock();
        emit pluginUnloaded(pluginName);

        return true;

    } catch (const std::exception& e) {
        emitError(pluginName, QString("Exception during unloading: %1").arg(e.what()));
        return false;
    } catch (...) {
        emitError(pluginName, "Unknown error during unloading");
        return false;
    }
}

void PluginManager::loadAllPlugins(const QString& pluginsDirectory) noexcept
{
    try {
        m_pluginsDirectory = pluginsDirectory;
        const QDir dir(pluginsDirectory);

        if (!dir.exists()) {
            emitError("", QString("Plugins directory does not exist: %1").arg(pluginsDirectory));
            return;
        }

        QStringList filters;
#ifdef Q_OS_WIN
        filters << "*.dll";
#elif defined(Q_OS_MAC)
        filters << "*.dylib";
#else
        filters << "*.so";
#endif

        const QStringList pluginFiles = dir.entryList(filters, QDir::Files);
        for (const QString& fileName : pluginFiles) {
            const QString filePath = dir.absoluteFilePath(fileName);
            loadPlugin(filePath);
        }

    } catch (const std::exception& e) {
        emitError("", QString("Exception during bulk loading: %1").arg(e.what()));
    } catch (...) {
        emitError("", "Unknown error during bulk loading");
    }
}

void PluginManager::unloadAllPlugins() noexcept
{
    try {
        // Get list of plugin names to avoid iterator invalidation
        QStringList pluginNames;

        {
            QReadLocker locker(&m_pluginsLock);
            pluginNames.reserve(static_cast<int>(m_loadedPlugins.size()));
            for (const auto& [name, info] : m_loadedPlugins) {
                pluginNames.append(name);
            }
        }

        // Unload each plugin
        for (const QString& name : pluginNames) {
            unloadPlugin(name);
        }

    } catch (const std::exception& e) {
        emitError("", QString("Exception during bulk unloading: %1").arg(e.what()));
    } catch (...) {
        emitError("", "Unknown error during bulk unloading");
    }
}

QStringList PluginManager::availablePlugins() const
{
    try {
        QReadLocker locker(&m_pluginsLock);

        QStringList result;
        result.reserve(static_cast<int>(m_loadedPlugins.size()));

        for (const auto& [name, info] : m_loadedPlugins) {
            result.append(name);
        }

        return result;

    } catch (...) {
        return {};
    }
}

Plugins::IPlugin* PluginManager::getPlugin(const QString& name) const
{
    QReadLocker locker(&m_pluginsLock);
    return getPluginUnsafe(name);
}

bool PluginManager::isPluginLoaded(const QString& name) const noexcept
{
    try {
        QReadLocker locker(&m_pluginsLock);
        return m_loadedPlugins.contains(name);
    } catch (...) {
        return false;
    }
}

bool PluginManager::isPluginEnabled(const QString& name) const noexcept
{
    try {
        QReadLocker locker(&m_pluginsLock);

        auto it = m_loadedPlugins.find(name);
        return (it != m_loadedPlugins.end()) ?
               it->second.enabled.load(std::memory_order_acquire) : false;

    } catch (...) {
        return false;
    }
}

bool PluginManager::enablePlugin(const QString& name) noexcept
{
    try {
        QReadLocker locker(&m_pluginsLock);

        auto it = m_loadedPlugins.find(name);
        if (it == m_loadedPlugins.end()) {
            return false;
        }

        if (it->second.enabled.load(std::memory_order_acquire)) {
            return true; // Already enabled
        }

        auto* plugin = qobject_cast<Plugins::IPlugin*>(it->second.pluginObject);
        if (!plugin) {
            return false;
        }

        // Unlock for plugin operation
        locker.unlock();

        if (plugin->initialize()) {
            // Atomically update enabled state
            it->second.enabled.store(true, std::memory_order_release);
            emit pluginEnabled(name);
            return true;
        }

        return false;

    } catch (const std::exception& e) {
        emitError(name, QString("Enable failed: %1").arg(e.what()));
        return false;
    } catch (...) {
        emitError(name, "Unknown error during enable");
        return false;
    }
}

bool PluginManager::disablePlugin(const QString& name) noexcept
{
    try {
        QReadLocker locker(&m_pluginsLock);

        auto it = m_loadedPlugins.find(name);
        if (it == m_loadedPlugins.end()) {
            return false;
        }

        if (!it->second.enabled.load(std::memory_order_acquire)) {
            return true; // Already disabled
        }

        auto* plugin = qobject_cast<Plugins::IPlugin*>(it->second.pluginObject);
        if (!plugin) {
            return false;
        }

        // Unlock for plugin operation
        locker.unlock();

        plugin->shutdown();
        it->second.enabled.store(false, std::memory_order_release);
        emit pluginDisabled(name);

        return true;

    } catch (const std::exception& e) {
        emitError(name, QString("Disable failed: %1").arg(e.what()));
        return false;
    } catch (...) {
        emitError(name, "Unknown error during disable");
        return false;
    }
}

int PluginManager::loadedPluginCount() const noexcept
{
    try {
        QReadLocker locker(&m_pluginsLock);
        return static_cast<int>(m_loadedPlugins.size());
    } catch (...) {
        return 0;
    }
}

int PluginManager::enabledPluginCount() const noexcept
{
    try {
        QReadLocker locker(&m_pluginsLock);

        int count = 0;
        for (const auto& [name, info] : m_loadedPlugins) {
            if (info.enabled.load(std::memory_order_acquire)) {
                ++count;
            }
        }
        return count;

    } catch (...) {
        return 0;
    }
}

bool PluginManager::validatePlugin(const Plugins::IPlugin* plugin) noexcept
{
    if (!plugin) {
        return false;
    }

    try {
        const QString name = plugin->name();
        const QString version = plugin->version();

        return !name.isEmpty() && !version.isEmpty();

    } catch (...) {
        return false;
    }
}

void PluginManager::initializePlugin(Plugins::IPlugin* plugin)
{
    if (!plugin) {
        throw std::invalid_argument("Null plugin provided");
    }

    // Connect plugin signals with proper lifetime management
    connect(plugin, &Plugins::IPlugin::statusChanged,
            this, [this, plugin](bool enabled) {
                const QString name = plugin->name();
                if (enabled) {
                    emit pluginEnabled(name);
                } else {
                    emit pluginDisabled(name);
                }
            }, Qt::QueuedConnection);

    connect(plugin, &Plugins::IPlugin::errorOccurred,
            this, [this, plugin](const QString& error) {
                emit pluginError(plugin->name(), error);
            }, Qt::QueuedConnection);

    if (!plugin->initialize()) {
        throw std::runtime_error("Plugin initialization failed");
    }
}

void PluginManager::emitError(const QString& pluginName, const QString& error) const noexcept
{
    try {
        // Use QueuedConnection for thread-safety
        QMetaObject::invokeMethod(const_cast<PluginManager*>(this),
                                  "pluginError",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, pluginName),
                                  Q_ARG(QString, error));
    } catch (...) {
        qWarning() << "PluginManager error:" << pluginName << "-" << error;
    }
}

Plugins::IPlugin* PluginManager::getPluginUnsafe(const QString& name) const noexcept
{
    try {
        auto it = m_loadedPlugins.find(name);
        if (it != m_loadedPlugins.end()) {
            return qobject_cast<Plugins::IPlugin*>(it->second.pluginObject);
        }
        return nullptr;

    } catch (...) {
        return nullptr;
    }
}

} // namespace DarkPlay::Core
