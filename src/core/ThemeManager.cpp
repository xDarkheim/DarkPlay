#include "core/ThemeManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutexLocker>

namespace DarkPlay::Core {

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_currentTheme("dark")
{
    try {
        registerBuiltinThemes();

        // Initialize with dark theme
        if (loadDarkTheme()) {
            m_initialized.store(true, std::memory_order_release);
        } else {
            emitError("Failed to initialize with default dark theme");
        }

    } catch (const std::exception& e) {
        emitError(QString("Exception in ThemeManager constructor: %1").arg(e.what()));
    } catch (...) {
        emitError("Unknown error in ThemeManager constructor");
    }
}

bool ThemeManager::loadTheme(const QString& themeName) noexcept
{
    if (themeName.isEmpty()) {
        emitError("Empty theme name provided");
        return false;
    }

    try {
        QReadLocker themesLocker(&m_themesLock);

        auto it = m_themes.find(themeName);
        if (it == m_themes.end()) {
            emitError(QString("Theme not found: %1").arg(themeName));
            return false;
        }

        const ThemeData* themeData = it->second.get(); // Fix: use ->second instead of .value()
        if (!themeData || !validateThemeData(*themeData)) {
            emitError(QString("Invalid theme data: %1").arg(themeName));
            return false;
        }

        // Create a copy of theme data for current theme
        auto newCurrentTheme = std::make_unique<ThemeData>();
        newCurrentTheme->name = themeData->name;
        newCurrentTheme->styleSheet = themeData->styleSheet;
        newCurrentTheme->colors = themeData->colors;

        themesLocker.unlock();

        // Update current theme atomically
        {
            QMutexLocker currentLocker(&m_currentThemeMutex);
            m_currentTheme = themeName;
            m_currentThemeData = std::move(newCurrentTheme);
        }

        // Apply stylesheet to application with error handling
        try {
            if (qApp) {
                qApp->setStyleSheet(m_currentThemeData->styleSheet);
            }
        } catch (const std::exception& e) {
            emitError(QString("Failed to apply stylesheet: %1").arg(e.what()));
            return false;
        }

        emit themeChanged(themeName);
        emit styleSheetChanged(m_currentThemeData->styleSheet);

        return true;

    } catch (const std::exception& e) {
        emitError(QString("Exception loading theme '%1': %2").arg(themeName, e.what()));
        return false;
    } catch (...) {
        emitError(QString("Unknown error loading theme '%1'").arg(themeName));
        return false;
    }
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
    try {
        QReadLocker locker(&m_themesLock);

        QStringList result;
        result.reserve(static_cast<int>(m_themes.size()));

        for (const auto& [name, themeData] : m_themes) {
            result.append(name);
        }

        return result;
    } catch (...) {
        return {};
    }
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

bool ThemeManager::loadDarkTheme() noexcept
{
    return loadTheme("dark");
}

bool ThemeManager::loadLightTheme() noexcept
{
    return loadTheme("light");
}

bool ThemeManager::isThemeValid(const QString& themeName) const noexcept
{
    if (themeName.isEmpty()) {
        return false;
    }

    try {
        QReadLocker locker(&m_themesLock);
        auto it = m_themes.find(themeName);
        return (it != m_themes.end()) && validateThemeData(*it->second);
    } catch (...) {
        return false;
    }
}

void ThemeManager::registerBuiltinThemes() noexcept
{
    try {
        // Dark theme
        auto darkTheme = std::make_unique<ThemeData>();
        darkTheme->name = "dark";
        darkTheme->colors = QJsonObject{
            {"background", "#1e1e1e"},
            {"surface", "#2d2d2d"},
            {"primary", "#0078d4"},
            {"secondary", "#404040"},
            {"text", "#ffffff"},
            {"textSecondary", "#cccccc"},
            {"border", "#333333"},
            {"accent", "#106ebe"},
            {"error", "#e74c3c"},
            {"success", "#27ae60"},
            {"warning", "#f39c12"}
        };

        darkTheme->styleSheet = R"(
            QMainWindow {
                background-color: #1e1e1e;
                color: #ffffff;
            }

            QWidget {
                background-color: #1e1e1e;
                color: #ffffff;
            }

            QPushButton {
                background-color: #404040;
                color: #ffffff;
                border: none;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }

            QPushButton:hover {
                background-color: #505050;
            }

            QPushButton:pressed {
                background-color: #303030;
            }

            QPushButton#playButton {
                background-color: #0078d4;
                border-radius: 25px;
                width: 50px;
                height: 50px;
                font-size: 16px;
            }

            QPushButton#playButton:hover {
                background-color: #106ebe;
            }

            QSlider::groove:horizontal {
                height: 6px;
                background: #404040;
                border-radius: 3px;
            }

            QSlider::handle:horizontal {
                background: #0078d4;
                width: 16px;
                height: 16px;
                border-radius: 8px;
                margin: -5px 0;
            }

            QSlider::sub-page:horizontal {
                background: #0078d4;
                border-radius: 3px;
            }

            QLabel {
                color: #ffffff;
            }

            QMenuBar {
                background-color: #2d2d2d;
                color: #ffffff;
            }

            QMenuBar::item:selected {
                background-color: #0078d4;
            }

            QMenu {
                background-color: #2d2d2d;
                color: #ffffff;
                border: 1px solid #404040;
            }

            QMenu::item:selected {
                background-color: #0078d4;
            }

            QStatusBar {
                background-color: #2d2d2d;
                color: #ffffff;
            }
        )";

        // Light theme
        auto lightTheme = std::make_unique<ThemeData>();
        lightTheme->name = "light";
        lightTheme->colors = QJsonObject{
            {"background", "#ffffff"},
            {"surface", "#f5f5f5"},
            {"primary", "#0078d4"},
            {"secondary", "#e0e0e0"},
            {"text", "#000000"},
            {"textSecondary", "#666666"},
            {"border", "#cccccc"},
            {"accent", "#106ebe"},
            {"error", "#e74c3c"},
            {"success", "#27ae60"},
            {"warning", "#f39c12"}
        };

        lightTheme->styleSheet = R"(
            QMainWindow {
                background-color: #ffffff;
                color: #000000;
            }

            QWidget {
                background-color: #ffffff;
                color: #000000;
            }

            QPushButton {
                background-color: #e0e0e0;
                color: #000000;
                border: 1px solid #cccccc;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }

            QPushButton:hover {
                background-color: #d0d0d0;
            }

            QPushButton:pressed {
                background-color: #c0c0c0;
            }

            QPushButton#playButton {
                background-color: #0078d4;
                color: #ffffff;
                border: none;
                border-radius: 25px;
                width: 50px;
                height: 50px;
                font-size: 16px;
            }

            QPushButton#playButton:hover {
                background-color: #106ebe;
            }

            QSlider::groove:horizontal {
                height: 6px;
                background: #e0e0e0;
                border-radius: 3px;
            }

            QSlider::handle:horizontal {
                background: #0078d4;
                width: 16px;
                height: 16px;
                border-radius: 8px;
                margin: -5px 0;
            }

            QSlider::sub-page:horizontal {
                background: #0078d4;
                border-radius: 3px;
            }

            QLabel {
                color: #000000;
            }

            QMenuBar {
                background-color: #f5f5f5;
                color: #000000;
            }

            QMenuBar::item:selected {
                background-color: #0078d4;
                color: #ffffff;
            }

            QMenu {
                background-color: #ffffff;
                color: #000000;
                border: 1px solid #cccccc;
            }

            QMenu::item:selected {
                background-color: #0078d4;
                color: #ffffff;
            }

            QStatusBar {
                background-color: #f5f5f5;
                color: #000000;
            }
        )";

        // Store themes with thread safety using std::unordered_map
        QWriteLocker locker(&m_themesLock);
        m_themes["dark"] = std::move(darkTheme);
        m_themes["light"] = std::move(lightTheme);

    } catch (const std::exception& e) {
        emitError(QString("Exception registering builtin themes: %1").arg(e.what()));
    } catch (...) {
        emitError("Unknown error registering builtin themes");
    }
}

QString ThemeManager::loadStyleSheetFromFile(const QString& filePath) noexcept
{
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open stylesheet file:" << filePath;
            return {};
        }

        return QString::fromUtf8(file.readAll());

    } catch (const std::exception& e) {
        qWarning() << "Exception loading stylesheet from" << filePath << ":" << e.what();
        return {};
    } catch (...) {
        qWarning() << "Unknown error loading stylesheet from" << filePath;
        return {};
    }
}

bool ThemeManager::validateThemeData(const ThemeData& theme) noexcept
{
    try {
        return !theme.name.isEmpty() &&
               !theme.styleSheet.isEmpty() &&
               !theme.colors.isEmpty();
    } catch (...) {
        return false;
    }
}

void ThemeManager::emitError(const QString& error) const noexcept
{
    try {
        // Use QueuedConnection for thread-safety
        QMetaObject::invokeMethod(const_cast<ThemeManager*>(this),
                                  "themeError",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, error));
    } catch (...) {
        qWarning() << "ThemeManager error:" << error;
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
