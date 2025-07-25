#include "core/Application.h"
#include "core/PluginManager.h"
#include "core/ThemeManager.h"
#include "core/ConfigManager.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace DarkPlay::Core {

std::atomic<Application*> Application::s_instance{nullptr};
std::mutex Application::s_instanceMutex;

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    // Thread-safe singleton initialization with double-checked locking
    {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        if (s_instance.load() == nullptr) {
            s_instance.store(this);
        } else {
            qWarning() << "Multiple Application instances created - this may cause issues";
        }
    }

    // Set application metadata
    setApplicationName("DarkPlay");
    setApplicationVersion("0.0.1");
    setOrganizationName("DarkPlay");
    setOrganizationDomain("darkheim.net");

    // Connect signal with proper error handling
    connect(this, &QApplication::aboutToQuit, this, &Application::onAboutToQuit);
}

Application::~Application()
{
    shutdown();

    // Clear singleton instance safely
    {
        std::lock_guard<std::mutex> lock(s_instanceMutex);
        if (s_instance.load() == this) {
            s_instance.store(nullptr);
        }
    }
}

Application* Application::instance() noexcept
{
    return s_instance.load(std::memory_order_acquire);
}

bool Application::initialize() noexcept
{
    // Thread-safe initialization with double-checked locking
    if (m_initialized.load(std::memory_order_acquire)) {
        return true; // Already initialized
    }

    std::lock_guard<std::mutex> lock(m_initializationMutex);

    // Double-check after acquiring lock
    if (m_initialized.load(std::memory_order_relaxed)) {
        return true;
    }

    try {
        initializeCore();
        validateCoreComponents();
        loadPlugins();

        m_initialized.store(true, std::memory_order_release);
        return true;
    } catch (const std::exception& e) {
        qCritical() << "Failed to initialize application:" << e.what();
        emit initializationFailed(QString::fromStdString(e.what()));
        return false;
    } catch (...) {
        qCritical() << "Unknown error during application initialization";
        emit initializationFailed("Unknown error during initialization");
        return false;
    }
}

void Application::initializeCore()
{
    // Initialize configuration manager first - it's required by others
    m_configManager = std::make_unique<ConfigManager>(this);
    if (!m_configManager) {
        throw std::runtime_error("Failed to create ConfigManager");
    }

    try {
        m_configManager->loadDefaults();
    } catch (const std::exception& e) {
        throw std::runtime_error(QString("Failed to load config defaults: %1").arg(e.what()).toStdString());
    }

    // Initialize theme manager with dependency injection
    m_themeManager = std::make_unique<ThemeManager>(this);
    if (!m_themeManager) {
        throw std::runtime_error("Failed to create ThemeManager");
    }

    // Load saved theme or default to dark - with error handling
    try {
        QString savedTheme = m_configManager->getValue("ui/theme", "dark").toString();
        m_themeManager->loadTheme(savedTheme);
    } catch (const std::exception& e) {
        qWarning() << "Failed to load theme, using default:" << e.what();
        m_themeManager->loadTheme("dark");
    }

    // Initialize plugin manager last
    m_pluginManager = std::make_unique<PluginManager>(this);
    if (!m_pluginManager) {
        throw std::runtime_error("Failed to create PluginManager");
    }

    // Connect theme changes to config with proper lifetime management
    connect(m_themeManager.get(), &ThemeManager::themeChanged,
            this, [this](const QString& themeName) {
                // Use weak reference pattern for safety
                if (auto* config = m_configManager.get()) {
                    try {
                        config->setValue("ui/theme", themeName);
                    } catch (const std::exception& e) {
                        qWarning() << "Failed to save theme preference:" << e.what();
                    }
                }
            });
}

void Application::validateCoreComponents() const
{
    if (!m_configManager) {
        throw std::runtime_error("ConfigManager not initialized");
    }
    if (!m_themeManager) {
        throw std::runtime_error("ThemeManager not initialized");
    }
    if (!m_pluginManager) {
        throw std::runtime_error("PluginManager not initialized");
    }
}

void Application::loadPlugins()
{
    validateCoreComponents();

    // Get plugins directory from config or use default
    QString pluginsDir;
    try {
        pluginsDir = m_configManager->getValue("plugins/directory", "plugins").toString();
    } catch (const std::exception& e) {
        qWarning() << "Failed to get plugins directory from config:" << e.what();
        pluginsDir = "plugins";
    }

    // Try application directory first, then system locations
    const QStringList searchPaths = {
        QDir::currentPath() + "/" + pluginsDir,
        QDir(applicationDirPath()).absoluteFilePath(pluginsDir),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + pluginsDir
    };

    bool pluginsLoaded = false;
    for (const QString& path : searchPaths) {
        const QDir pluginDir(path);
        if (pluginDir.exists()) {
            try {
                m_pluginManager->loadAllPlugins(path);
                pluginsLoaded = true;
                qDebug() << "Successfully loaded plugins from:" << path;
                break;
            } catch (const std::exception& e) {
                qWarning() << "Failed to load plugins from" << path << ":" << e.what();
                // Continue to next path
            }
        }
    }

    if (!pluginsLoaded) {
        qWarning() << "No plugins loaded from any search path";
    }
}

void Application::shutdown() noexcept
{
    if (!m_initialized.load(std::memory_order_acquire)) {
        return; // Already shut down or never initialized
    }

    std::lock_guard<std::mutex> lock(m_initializationMutex);

    // Double-check after acquiring lock
    if (!m_initialized.load(std::memory_order_relaxed)) {
        return;
    }

    try {
        emit aboutToQuit();

        // Shutdown in reverse order with proper error handling
        if (m_pluginManager) {
            try {
                m_pluginManager->unloadAllPlugins();
            } catch (const std::exception& e) {
                qWarning() << "Error unloading plugins:" << e.what();
            }
        }

        if (m_configManager) {
            try {
                m_configManager->sync();
            } catch (const std::exception& e) {
                qWarning() << "Error syncing configuration:" << e.what();
            }
        }

        // Clear smart pointers in reverse order
        m_pluginManager.reset();
        m_themeManager.reset();
        m_configManager.reset();

        m_initialized.store(false, std::memory_order_release);

    } catch (const std::exception& e) {
        qWarning() << "Error during shutdown:" << e.what();
    } catch (...) {
        qWarning() << "Unknown error during shutdown";
    }
}

void Application::onAboutToQuit()
{
    shutdown();
}

} // namespace DarkPlay::Core
