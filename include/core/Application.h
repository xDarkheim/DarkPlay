#ifndef DARKPLAY_CORE_APPLICATION_H
#define DARKPLAY_CORE_APPLICATION_H

#include <QApplication>
#include <memory>
#include <mutex>
#include <atomic>

namespace DarkPlay::Core {

// Forward declarations
class PluginManager;
class ThemeManager;
class ConfigManager;

/**
 * @brief Main application class that manages core systems
 * Thread-safe singleton with proper RAII and exception safety
 */
class Application : public QApplication
{
    Q_OBJECT

public:
    explicit Application(int &argc, char **argv);
    ~Application() override;

    // Delete copy and move constructors/operators for singleton
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Thread-safe singleton access
    [[nodiscard]] static Application* instance() noexcept;

    // Core system accessors with null checks and const correctness
    [[nodiscard]] PluginManager* pluginManager() const noexcept {
        return m_pluginManager.get();
    }

    [[nodiscard]] ThemeManager* themeManager() const noexcept {
        return m_themeManager.get();
    }

    [[nodiscard]] ConfigManager* configManager() const noexcept {
        return m_configManager.get();
    }

    // Application lifecycle with exception safety
    bool initialize() noexcept;
    void shutdown() noexcept;

    // State queries
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized.load(); }

signals:
    void aboutToQuit();
    void initializationFailed(const QString& error);

private slots:
    void onAboutToQuit();

private:
    void initializeCore();
    void loadPlugins();

    // Ensure proper initialization order
    void validateCoreComponents() const;

    std::unique_ptr<ConfigManager> m_configManager;
    std::unique_ptr<ThemeManager> m_themeManager;
    std::unique_ptr<PluginManager> m_pluginManager;

    static std::atomic<Application*> s_instance;
    static std::mutex s_instanceMutex;
    std::atomic<bool> m_initialized{false};

    // Track initialization state for better error handling
    mutable std::mutex m_initializationMutex;
};

} // namespace DarkPlay::Core

#endif // DARKPLAY_CORE_APPLICATION_H
