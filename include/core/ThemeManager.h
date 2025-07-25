#ifndef DARKPLAY_CORE_THEMEMANAGER_H
#define DARKPLAY_CORE_THEMEMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QMutex>
#include <QReadWriteLock>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace DarkPlay::Core {

/**
 * @brief Thread-safe manager for application themes and styling
 * Provides RAII-compliant theme management with proper error handling
 */
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() override = default;

    // Delete copy and move operations for safety
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    ThemeManager(ThemeManager&&) = delete;
    ThemeManager& operator=(ThemeManager&&) = delete;

    // Theme management - thread-safe with proper error handling
    bool loadTheme(const QString& themeName) noexcept;
    bool loadThemeFromFile(const QString& filePath) noexcept;

    // Theme queries - thread-safe
    [[nodiscard]] QStringList availableThemes() const;
    [[nodiscard]] QString currentTheme() const noexcept;

    // Style access - thread-safe
    [[nodiscard]] QString getStyleSheet() const;
    [[nodiscard]] QJsonObject getColors() const;
    [[nodiscard]] QString getColor(const QString& colorName) const;

    // Built-in themes
    bool loadDarkTheme() noexcept;
    bool loadLightTheme() noexcept;

    // Validation
    [[nodiscard]] bool isThemeValid(const QString& themeName) const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized.load(); }

signals:
    void themeChanged(const QString& themeName);
    void styleSheetChanged(const QString& styleSheet);
    void themeError(const QString& error);

private:
    struct ThemeData {
        QString name;
        QString styleSheet;
        QJsonObject colors;

        // Make it movable but not copyable
        ThemeData() = default;
        ThemeData(const ThemeData&) = delete;
        ThemeData& operator=(const ThemeData&) = delete;
        ThemeData(ThemeData&&) = default;
        ThemeData& operator=(ThemeData&&) = default;
    };

    // Thread-safe container access - use std::unordered_map instead of QHash
    mutable QReadWriteLock m_themesLock;
    std::unordered_map<QString, std::unique_ptr<ThemeData>> m_themes;

    mutable QMutex m_currentThemeMutex;
    QString m_currentTheme;
    std::unique_ptr<ThemeData> m_currentThemeData;

    std::atomic<bool> m_initialized{false};

    // Helper methods
    void registerBuiltinThemes() noexcept;
    [[nodiscard]] static QString loadStyleSheetFromFile(const QString& filePath) noexcept;
    [[nodiscard]] static bool validateThemeData(const ThemeData& theme) noexcept;
    void emitError(const QString& error) const noexcept;

    // Safe theme access
    [[nodiscard]] const ThemeData* getThemeDataUnsafe(const QString& themeName) const noexcept;
};

} // namespace DarkPlay::Core

#endif // DARKPLAY_CORE_THEMEMANAGER_H
