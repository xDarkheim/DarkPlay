#include "core/ThemeManager.h"

#include <QApplication>
#include <QStyleHints>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QWriteLocker>
#include <QMutexLocker>
#include <QWidget>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

namespace DarkPlay::Core {

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_currentThemeType(ThemeType::Auto)
{
    detectSystemTheme();
    setupSystemThemeWatching();

    // Load auto theme by default
    loadAutoTheme();

    m_initialized.store(true, std::memory_order_release);
}

bool ThemeManager::loadTheme(const QString& themeName) noexcept
{
    if (themeName.isEmpty()) {
        emitError("Empty theme name provided");
        return false;
    }

    if (!isThemeValid(themeName)) {
        emitError(QString("Invalid theme name: %1").arg(themeName));
        return false;
    }

    return loadAutoTheme();
}

bool ThemeManager::loadTheme(ThemeType themeType) noexcept
{
    if (themeType != ThemeType::Auto) {
        emitError("Only Auto theme type is supported");
        return false;
    }

    return loadAutoTheme();
}

bool ThemeManager::loadAutoTheme() noexcept
{
    try {
        auto themeData = std::make_unique<ThemeData>();
        themeData->name = "auto";
        themeData->type = ThemeType::Auto;

        // Apply system theme colors and palette
        bool isDark = m_isSystemDark.load();
        themeData->palette = isDark ? createDarkPalette() : createLightPalette();

        // Set basic theme colors
        QJsonObject colors;
        if (isDark) {
            colors["background"] = "#2b2b2b";
            colors["foreground"] = "#ffffff";
            colors["accent"] = "#4dabf7";
        } else {
            colors["background"] = "#ffffff";
            colors["foreground"] = "#000000";
            colors["accent"] = "#0066cc";
        }
        themeData->colors = colors;

        // Apply the theme
        {
            QMutexLocker locker(&m_currentThemeMutex);
            m_currentTheme = themeData->name;
            m_currentThemeType = themeData->type;
            m_currentThemeData = std::move(themeData);
        }

        // Apply palette to application
        QApplication::setPalette(m_currentThemeData->palette);

        emit themeChanged(m_currentTheme);
        emit themeTypeChanged(m_currentThemeType);
        emit styleSheetChanged(m_currentThemeData->styleSheet);

        return true;

    } catch (const std::exception& e) {
        emitError(QString("Exception loading auto theme: %1").arg(e.what()));
        return false;
    } catch (...) {
        emitError("Unknown error loading auto theme");
        return false;
    }
}

ThemeManager::ThemeType ThemeManager::currentThemeType() const noexcept
{
    try {
        QMutexLocker locker(&m_currentThemeMutex);
        return m_currentThemeType;
    } catch (...) {
        return ThemeType::Auto;
    }
}

void ThemeManager::enableSystemThemeAdaptation(bool enabled) noexcept
{
    m_systemThemeAdaptation.store(enabled, std::memory_order_release);

    if (enabled && m_currentThemeType == ThemeType::Auto) {
        applySystemTheme();
    }
}

bool ThemeManager::isSystemThemeAdaptationEnabled() const noexcept
{
    return m_systemThemeAdaptation.load(std::memory_order_acquire);
}

bool ThemeManager::isSystemDarkTheme() const noexcept
{
    return m_isSystemDark.load(std::memory_order_acquire);
}

void ThemeManager::applySystemTheme() noexcept
{
    if (m_currentThemeType == ThemeType::Auto) {
        loadAutoTheme();
    }
}

void ThemeManager::adaptWindowFrame(QWidget* window) const noexcept
{
    if (!window) {
        return;
    }

    try {
        bool isDark = m_isSystemDark.load();

#ifdef Q_OS_WIN
        // Windows-specific window frame adaptation would go here
        Q_UNUSED(window)
        Q_UNUSED(isDark)
#elif defined(Q_OS_MACOS)
        // macOS-specific window frame adaptation would go here
        Q_UNUSED(window)
        Q_UNUSED(isDark)
#else
        // Linux/other platforms
        Q_UNUSED(window)
        Q_UNUSED(isDark)
#endif

    } catch (const std::exception& e) {
        qDebug() << "ThemeManager: Error adapting window frame:" << e.what();
    } catch (...) {
        qDebug() << "ThemeManager: Unknown error adapting window frame";
    }
}

void ThemeManager::onSystemThemeChanged()
{
    detectSystemTheme();
    emit systemThemeChanged(m_isSystemDark.load());

    if (m_systemThemeAdaptation.load() && m_currentThemeType == ThemeType::Auto) {
        applySystemTheme();
    }
}


void ThemeManager::setupSystemThemeWatching() noexcept
{
    try {
        // Connect to Qt's style hints for theme changes
        if (QStyleHints* hints = QApplication::styleHints()) {
            connect(hints, &QStyleHints::colorSchemeChanged, this, &ThemeManager::onSystemThemeChanged);
        }

#ifdef Q_OS_WIN
        // Windows: Use registry monitoring for theme changes
        // This is handled by Qt's QStyleHints::colorSchemeChanged
#endif

        qDebug() << "ThemeManager: System theme watching enabled";

    } catch (const std::exception& e) {
        qDebug() << "ThemeManager: Error setting up system theme watching:" << e.what();
    }
}

void ThemeManager::detectSystemTheme() noexcept
{
    try {
        bool isDark = false;

#ifdef Q_OS_LINUX
        // Fallback: Check GTK settings
        QSettings gtkSettings(QDir::homePath() + "/.config/gtk-3.0/settings.ini", QSettings::IniFormat);
        QString themeName = gtkSettings.value("Settings/gtk-theme-name").toString().toLower();
        isDark = themeName.contains("dark") || themeName.contains("adwaita-dark");
#endif

#ifdef Q_OS_WIN
        // Windows: Check registry for dark mode preference
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          QSettings::NativeFormat);
        int appsUseLightTheme = settings.value("AppsUseLightTheme", 1).toInt();
        isDark = (appsUseLightTheme == 0);
#endif

#ifdef Q_OS_MACOS
        // macOS: Check system appearance
        // This is handled by Qt's QStyleHints::colorScheme()
        if (QStyleHints* hints = QApplication::styleHints()) {
            isDark = (hints->colorScheme() == Qt::ColorScheme::Dark);
        }
#endif

        // Qt 6.5+ cross-platform method (primary detection method)
        if (QStyleHints* hints = QApplication::styleHints()) {
            Qt::ColorScheme colorScheme = hints->colorScheme();
            if (colorScheme != Qt::ColorScheme::Unknown) {
                isDark = (colorScheme == Qt::ColorScheme::Dark);
            }
        }

        m_isSystemDark.store(isDark, std::memory_order_release);
        qDebug() << "ThemeManager: Detected system theme - isDark:" << isDark;

    } catch (const std::exception& e) {
        qDebug() << "ThemeManager: Error detecting system theme:" << e.what();
        // Default to light theme on error
        m_isSystemDark.store(false, std::memory_order_release);
    }
}

QPalette ThemeManager::createLightPalette() const noexcept
{
    // Use system palette with minimal customization
    QPalette palette = QApplication::palette();
    
    // Only set essential colors for light theme consistency
    palette.setColor(QPalette::Highlight, QColor("#0066cc"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::Link, QColor("#0066cc"));
    palette.setColor(QPalette::LinkVisited, QColor("#004499"));

    return palette;
}

QPalette ThemeManager::createDarkPalette() const noexcept
{
    // Use system palette with minimal customization  
    QPalette palette = QApplication::palette();
    
    // Only set essential colors for dark theme consistency
    palette.setColor(QPalette::Highlight, QColor("#4dabf7"));
    palette.setColor(QPalette::HighlightedText, QColor("#000000"));
    palette.setColor(QPalette::Link, QColor("#4dabf7"));
    palette.setColor(QPalette::LinkVisited, QColor("#339af0"));


    return palette;
}

bool ThemeManager::loadThemeFromFile(const QString& filePath) noexcept
{
    if (filePath.isEmpty()) {
        emitError("Empty file path provided");
        return false;
    }

    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            emitError(QString("Cannot open theme file: %1").arg(filePath));
            return false;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);

        if (parseError.error != QJsonParseError::NoError) {
            emitError(QString("JSON parse error in %1: %2").arg(filePath, parseError.errorString()));
            return false;
        }

        const QJsonObject themeObj = doc.object();

        auto theme = std::make_unique<ThemeData>();
        theme->name = themeObj["name"].toString();
        theme->colors = themeObj["colors"].toObject();

        if (theme->name.isEmpty()) {
            emitError(QString("Theme name is empty in file: %1").arg(filePath));
            return false;
        }

        // Load stylesheet from a separate file or inline
        if (themeObj.contains("stylesheetFile")) {
            const QString styleSheetPath = QFileInfo(filePath).dir().absoluteFilePath(
                themeObj["stylesheetFile"].toString());
            theme->styleSheet = loadStyleSheetFromFile(styleSheetPath);
        } else {
            theme->styleSheet = themeObj["stylesheet"].toString();
        }

        if (!validateThemeData(*theme)) {
            emitError(QString("Invalid theme data in file: %1").arg(filePath));
            return false;
        }

        // Store the theme
        {
            QWriteLocker locker(&m_themesLock);
            m_themes[theme->name] = std::move(theme);
        }

        return true;

    } catch (const std::exception& e) {
        emitError(QString("Exception loading theme from file '%1': %2").arg(filePath, e.what()));
        return false;
    } catch (...) {
        emitError(QString("Unknown error loading theme from file '%1'").arg(filePath));
        return false;
    }
}

QStringList ThemeManager::availableThemes() const
{
    // Only "auto" theme is available
    return QStringList{"auto"};
}

QString ThemeManager::currentTheme() const noexcept
{
    try {
        QMutexLocker locker(&m_currentThemeMutex);
        return m_currentTheme;
    } catch (...) {
        return {};
    }
}

QString ThemeManager::getStyleSheet() const
{
    try {
        QMutexLocker locker(&m_currentThemeMutex);
        return m_currentThemeData ? m_currentThemeData->styleSheet : QString{};
    } catch (...) {
        return {};
    }
}

QJsonObject ThemeManager::getColors() const
{
    try {
        QMutexLocker locker(&m_currentThemeMutex);
        return m_currentThemeData ? m_currentThemeData->colors : QJsonObject{};
    } catch (...) {
        return {};
    }
}

QString ThemeManager::getColor(const QString& colorName) const
{
    if (colorName.isEmpty()) {
        return {};
    }

    try {
        QMutexLocker locker(&m_currentThemeMutex);
        if (m_currentThemeData) {
            return m_currentThemeData->colors[colorName].toString();
        }
        return {};
    } catch (...) {
        return {};
    }
}

bool ThemeManager::isThemeValid(const QString& themeName) const noexcept
{
    // Only "auto" theme is valid
    return (themeName == "auto");
}

QString ThemeManager::loadStyleSheetFromFile(const QString& filePath) noexcept
{
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return file.readAll();
    } catch (...) {
        return {};
    }
}

bool ThemeManager::validateThemeData(const ThemeData& theme) noexcept
{
    return !theme.name.isEmpty();
}

void ThemeManager::emitError(const QString& error) const noexcept
{
    try {
        qWarning() << "ThemeManager Error:" << error;
        emit const_cast<ThemeManager*>(this)->themeError(error);
    } catch (...) {
        // Silent - we're in error reporting
    }
}

const ThemeManager::ThemeData* ThemeManager::getThemeDataUnsafe(const QString& themeName) const noexcept
{
    try {
        auto it = m_themes.find(themeName);
        return (it != m_themes.end()) ? it->second.get() : nullptr;
    } catch (...) {
        return nullptr;
    }
}

} // namespace DarkPlay::Core
