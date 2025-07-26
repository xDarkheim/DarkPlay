#ifndef DARKPLAY_CORE_THEMEMANAGER_H
#define DARKPLAY_CORE_THEMEMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QMutex>
#include <QReadWriteLock>
#include <QPalette>
#include <QStyleHints>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace DarkPlay::Core {

/**
 * @brief Thread-safe manager for application themes and styling
 * Provides RAII-compliant theme management with OS theme adaptation
 */
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum class ThemeType {
        Auto     // Follow system theme only
    };

    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() override = default;

    // Delete copy and move operations for safety
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;
    ThemeManager(ThemeManager&&) = delete;
    ThemeManager& operator=(ThemeManager&&) = delete;

    // Theme management - thread-safe with proper error handling
    bool loadTheme(const QString& themeName) noexcept;
    bool loadTheme(ThemeType themeType) noexcept;
    bool loadThemeFromFile(const QString& filePath) noexcept;

    // System theme detection and adaptation
    void enableSystemThemeAdaptation(bool enabled = true) noexcept;
    [[nodiscard]] bool isSystemThemeAdaptationEnabled() const noexcept;
    [[nodiscard]] bool isSystemDarkTheme() const noexcept;
    void applySystemTheme() noexcept;

    // Theme queries - thread-safe
    [[nodiscard]] QStringList availableThemes() const;
    [[nodiscard]] QString currentTheme() const noexcept;
    [[nodiscard]] ThemeType currentThemeType() const noexcept;

    // Style access - thread-safe
    [[nodiscard]] QString getStyleSheet() const;
    [[nodiscard]] QJsonObject getColors() const;
    [[nodiscard]] QString getColor(const QString& colorName) const;

    // Auto theme (follows system theme)
    bool loadAutoTheme() noexcept;

    // Validation
    [[nodiscard]] bool isThemeValid(const QString& themeName) const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized.load(); }

    // Window frame adaptation
    void adaptWindowFrame(QWidget* window) const noexcept;

signals:
    void themeChanged(const QString& themeName);
    void themeTypeChanged(ThemeType themeType);
    void styleSheetChanged(const QString& styleSheet);
    void systemThemeChanged(bool isDark);
    void themeError(const QString& error);

private slots:
    void onSystemThemeChanged();

private:
    struct ThemeData {
        QString name;
        ThemeType type;
        QString styleSheet;
        QJsonObject colors;
        QPalette palette;

        // Make it movable but not copyable
        ThemeData() = default;
        ThemeData(const ThemeData&) = delete;
        ThemeData& operator=(const ThemeData&) = delete;
        ThemeData(ThemeData&&) = default;
        ThemeData& operator=(ThemeData&&) = default;
    };

    // Thread-safe container access
    mutable QReadWriteLock m_themesLock;
    std::unordered_map<QString, std::unique_ptr<ThemeData>> m_themes;

    mutable QMutex m_currentThemeMutex;
    QString m_currentTheme;
    ThemeType m_currentThemeType;
    std::unique_ptr<ThemeData> m_currentThemeData;

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_systemThemeAdaptation{true};
    mutable std::atomic<bool> m_isSystemDark{false};

    // Helper methods
    void setupSystemThemeWatching() noexcept;

    [[nodiscard]] static QString loadStyleSheetFromFile(const QString& filePath) noexcept;
    [[nodiscard]] static bool validateThemeData(const ThemeData& theme) noexcept;
    [[nodiscard]] QPalette createLightPalette() const noexcept;
    [[nodiscard]] QPalette createDarkPalette() const noexcept;

    void emitError(const QString& error) const noexcept;
    void detectSystemTheme() noexcept;

    // Safe theme access
    [[nodiscard]] const ThemeData* getThemeDataUnsafe(const QString& themeName) const noexcept;
};

} // namespace DarkPlay::Core

#endif // DARKPLAY_CORE_THEMEMANAGER_H
