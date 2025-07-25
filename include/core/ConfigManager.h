#ifndef DARKPLAY_CORE_CONFIGMANAGER_H
#define DARKPLAY_CORE_CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QJsonObject>
#include <QVariant>
#include <QMutex>
#include <QReadWriteLock>
#include <memory>

namespace DarkPlay::Core {

/**
 * @brief Thread-safe manager for application configuration and settings
 * Provides RAII-compliant configuration management with proper error handling
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() override = default;

    // Delete copy and move operations for singleton-like behavior
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

    // Configuration access - thread-safe with proper const-correctness
    [[nodiscard]] QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    bool setValue(const QString& key, const QVariant& value) noexcept;
    [[nodiscard]] bool contains(const QString& key) const noexcept;
    bool remove(const QString& key) noexcept;

    // Sections management - thread-safe
    bool beginGroup(const QString& prefix) noexcept;
    void endGroup() noexcept;
    [[nodiscard]] QStringList childKeys() const;
    [[nodiscard]] QStringList childGroups() const;

    // JSON configuration with validation
    [[nodiscard]] QJsonObject getSection(const QString& section) const;
    bool setSection(const QString& section, const QJsonObject& data) noexcept;

    // File operations with error handling
    bool sync() noexcept;
    [[nodiscard]] QString fileName() const noexcept;

    // Default configuration
    bool loadDefaults() noexcept;
    bool resetToDefaults() noexcept;

    // Validation
    [[nodiscard]] bool isValidKey(const QString& key) const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

signals:
    void configChanged(const QString& key, const QVariant& value);
    void sectionChanged(const QString& section);
    void errorOccurred(const QString& error);

private:
    void setupDefaults() noexcept;
    [[nodiscard]] static QJsonObject variantMapToJson(const QVariantMap& map) noexcept;
    [[nodiscard]] static QVariantMap jsonToVariantMap(const QJsonObject& json) noexcept;

    // Validation helpers
    [[nodiscard]] bool validateValue(const QString& key, const QVariant& value) const noexcept;
    void emitError(const QString& error) const noexcept;

    std::unique_ptr<QSettings> m_settings;
    mutable QReadWriteLock m_lock; // Read-write lock for better performance
    std::atomic<bool> m_initialized{false};

    // Track group state for proper nesting
    mutable QString m_currentGroup;
    mutable int m_groupDepth{0};
};

} // namespace DarkPlay::Core

#endif // DARKPLAY_CORE_CONFIGMANAGER_H
