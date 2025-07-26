#include "core/ConfigManager.h"
#include <QStandardPaths>
#include <QJsonDocument>
#include <QDir>
#include <QDebug>
#include <QReadLocker>
#include <QWriteLocker>

namespace DarkPlay::Core {

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    try {
        // Ensure config directory exists
        const QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        const QDir configDir(configPath);
        
        if (!configDir.exists() && !configDir.mkpath(".")) {
            emitError(QString("Failed to create config directory: %1").arg(configPath));
            return;
        }

        const QString configFile = configPath + "/DarkPlay.conf";
        
        // Create settings with error handling
        m_settings = std::make_unique<QSettings>(configFile, QSettings::IniFormat);
        
        if (m_settings->status() != QSettings::NoError) {
            emitError(QString("Failed to initialize settings file: %1").arg(configFile));
            return;
        }

        setupDefaults();
        m_initialized.store(true, std::memory_order_release);
        
    } catch (const std::exception& e) {
        emitError(QString("Exception in ConfigManager constructor: %1").arg(e.what()));
    } catch (...) {
        emitError("Unknown error in ConfigManager constructor");
    }
}

QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    if (!isValidKey(key)) {
        return defaultValue;
    }

    QReadLocker locker(&m_lock);
    
    if (!m_settings) {
        return defaultValue;
    }

    try {
        return m_settings->value(key, defaultValue);
    } catch (const std::exception& e) {
        emitError(QString("Error reading key '%1': %2").arg(key, e.what()));
        return defaultValue;
    } catch (...) {
        emitError(QString("Unknown error reading key '%1'").arg(key));
        return defaultValue;
    }
}

bool ConfigManager::setValue(const QString& key, const QVariant& value) noexcept
{
    if (!isValidKey(key) || !validateValue(key, value)) {
        return false;
    }

    try {
        QWriteLocker locker(&m_lock);
        
        if (!m_settings) {
            emitError("Settings not initialized");
            return false;
        }

        const QVariant oldValue = m_settings->value(key);

        // Use strong exception safety - only commit if operation succeeds
        m_settings->setValue(key, value);

        // Ensure write is successful before emitting signal
        if (m_settings->status() == QSettings::NoError) {
            // Only emit if value actually changed
            if (oldValue != value) {
                emit configChanged(key, value);
            }
            return true;
        } else {
            emitError(QString("Failed to write key '%1': Settings error").arg(key));
            return false;
        }

    } catch (const std::exception& e) {
        emitError(QString("Exception writing key '%1': %2").arg(key, e.what()));
        return false;
    } catch (...) {
        emitError(QString("Unknown exception writing key '%1'").arg(key));
        return false;
    }
}

bool ConfigManager::contains(const QString& key) const noexcept
{
    if (!isValidKey(key)) {
        return false;
    }

    try {
        QReadLocker locker(&m_lock);
        return m_settings ? m_settings->contains(key) : false;
    } catch (...) {
        return false;
    }
}

bool ConfigManager::remove(const QString& key) noexcept
{
    if (!isValidKey(key)) {
        return false;
    }

    try {
        QWriteLocker locker(&m_lock);
        
        if (!m_settings || !m_settings->contains(key)) {
            return false;
        }

        m_settings->remove(key);
        
        const QString section = key.split('/').first();
        
        // Emit signal outside the lock
        locker.unlock();
        
        if (!section.isEmpty()) {
            emit sectionChanged(section);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        emitError(QString("Error removing key '%1': %2").arg(key, e.what()));
        return false;
    } catch (...) {
        emitError(QString("Unknown error removing key '%1'").arg(key));
        return false;
    }
}

bool ConfigManager::beginGroup(const QString& prefix) noexcept
{
    if (prefix.isEmpty()) {
        emitError("Empty group prefix provided");
        return false;
    }

    try {
        QWriteLocker locker(&m_lock);
        
        if (!m_settings) {
            emitError("Settings not initialized");
            return false;
        }

        m_settings->beginGroup(prefix);
        m_currentGroup = prefix;
        ++m_groupDepth;
        
        return true;
        
    } catch (const std::exception& e) {
        emitError(QString("Error beginning group '%1': %2").arg(prefix, e.what()));
        return false;
    } catch (...) {
        emitError(QString("Unknown error beginning group '%1'").arg(prefix));
        return false;
    }
}

void ConfigManager::endGroup() noexcept
{
    try {
        QWriteLocker locker(&m_lock);
        
        if (!m_settings) {
            return;
        }

        if (m_groupDepth > 0) {
            m_settings->endGroup();
            --m_groupDepth;
            
            if (m_groupDepth == 0) {
                m_currentGroup.clear();
            }
        }
        
    } catch (const std::exception& e) {
        emitError(QString("Error ending group: %1").arg(e.what()));
    } catch (...) {
        emitError("Unknown error ending group");
    }
}

QStringList ConfigManager::childKeys() const
{
    try {
        QReadLocker locker(&m_lock);
        return m_settings ? m_settings->childKeys() : QStringList{};
    } catch (const std::exception& e) {
        emitError(QString("Error getting child keys: %1").arg(e.what()));
        return {};
    } catch (...) {
        emitError("Unknown error getting child keys");
        return {};
    }
}

QStringList ConfigManager::childGroups() const
{
    try {
        QReadLocker locker(&m_lock);
        return m_settings ? m_settings->childGroups() : QStringList{};
    } catch (const std::exception& e) {
        emitError(QString("Error getting child groups: %1").arg(e.what()));
        return {};
    } catch (...) {
        emitError("Unknown error getting child groups");
        return {};
    }
}

QJsonObject ConfigManager::getSection(const QString& section) const
{
    if (section.isEmpty()) {
        emitError("Empty section name provided");
        return {};
    }

    try {
        QReadLocker locker(&m_lock);
        
        if (!m_settings) {
            emitError("Settings not initialized");
            return {};
        }

        m_settings->beginGroup(section);
        const QStringList keys = m_settings->childKeys();

        QVariantMap sectionData;
        for (const QString& key : keys) {
            sectionData[key] = m_settings->value(key);
        }

        m_settings->endGroup();
        return variantMapToJson(sectionData);
        
    } catch (const std::exception& e) {
        emitError(QString("Error getting section '%1': %2").arg(section, e.what()));
        return {};
    } catch (...) {
        emitError(QString("Unknown error getting section '%1'").arg(section));
        return {};
    }
}

bool ConfigManager::setSection(const QString& section, const QJsonObject& data) noexcept
{
    if (section.isEmpty()) {
        emitError("Empty section name provided");
        return false;
    }

    try {
        QWriteLocker locker(&m_lock);
        
        if (!m_settings) {
            emitError("Settings not initialized");
            return false;
        }

        // Use RAII for group management
        class GroupGuard {
        public:
            GroupGuard(QSettings* settings, const QString& group)
                : m_settings(settings), m_hasGroup(!group.isEmpty()) {
                if (m_hasGroup) {
                    m_settings->beginGroup(group);
                }
            }

            ~GroupGuard() {
                if (m_hasGroup && m_settings) {
                    m_settings->endGroup();
                }
            }

        private:
            QSettings* m_settings;
            bool m_hasGroup;
        };

        // Convert JSON to variant map for atomic operation
        const QVariantMap variantMap = jsonToVariantMap(data);

        // Ensure atomic write operation
        GroupGuard guard(m_settings.get(), section);

        // Clear existing keys in section first
        m_settings->remove("");

        // Write all new values
        for (auto it = variantMap.constBegin(); it != variantMap.constEnd(); ++it) {
            m_settings->setValue(it.key(), it.value());
        }

        if (m_settings->status() == QSettings::NoError) {
            emit sectionChanged(section);
            return true;
        } else {
            emitError(QString("Failed to write section '%1'").arg(section));
            return false;
        }

    } catch (const std::exception& e) {
        emitError(QString("Exception writing section '%1': %2").arg(section, e.what()));
        return false;
    } catch (...) {
        emitError(QString("Unknown exception writing section '%1'").arg(section));
        return false;
    }
}

bool ConfigManager::sync() noexcept
{
    try {
        QWriteLocker locker(&m_lock);

        if (!m_settings) {
            emitError("Settings not initialized");
            return false;
        }

        m_settings->sync();
        
        if (m_settings->status() != QSettings::NoError) {
            emitError("Failed to sync settings to disk");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        emitError(QString("Exception during sync: %1").arg(e.what()));
        return false;
    } catch (...) {
        emitError("Unknown exception during sync");
        return false;
    }
}

QString ConfigManager::fileName() const noexcept
{
    try {
        QReadLocker locker(&m_lock);
        return m_settings ? m_settings->fileName() : QString{};
    } catch (...) {
        return {};
    }
}

bool ConfigManager::loadDefaults() noexcept
{
    setupDefaults();
    return true;
}

bool ConfigManager::resetToDefaults() noexcept
{
    try {
        QWriteLocker locker(&m_lock);
        
        if (!m_settings) {
            emitError("Settings not initialized");
            return false;
        }

        m_settings->clear();
        
        // Unlock before calling setupDefaults to prevent deadlock
        locker.unlock();
        
        setupDefaults();
        sync();
        
        return true;
        
    } catch (const std::exception& e) {
        emitError(QString("Error resetting to defaults: %1").arg(e.what()));
        return false;
    } catch (...) {
        emitError("Unknown error resetting to defaults");
        return false;
    }
}

bool ConfigManager::isValidKey(const QString& key) const noexcept
{
    return !key.isEmpty() && !key.contains("//") && !key.startsWith('/') && !key.endsWith('/');
}

void ConfigManager::setupDefaults() noexcept
{
    // Define all defaults in a structured way
    const QMap<QString, QVariant> defaults = {
        // UI defaults
        {"ui/theme", "dark"},
        {"ui/language", "en"},
        {"ui/windowGeometry", QByteArray()},
        {"ui/windowState", QByteArray()},
        
        // Media defaults
        {"media/volume", 0.7},
        {"media/muted", false},
        {"media/autoplay", true},
        {"media/defaultEngine", "qt"},
        
        // Plugins defaults
        {"plugins/directory", "plugins"},
        {"plugins/autoload", true},
        
        // Performance defaults
        {"performance/hwAcceleration", true},
        {"performance/bufferSize", 8192},
        
        // Recent files
        {"files/recentFiles", QStringList()},
        {"files/maxRecentFiles", 10},
        {"files/lastDirectory", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)}
    };

    try {
        for (auto it = defaults.begin(); it != defaults.end(); ++it) {
            if (!contains(it.key())) {
                setValue(it.key(), it.value());
            }
        }
    } catch (const std::exception& e) {
        emitError(QString("Error setting up defaults: %1").arg(e.what()));
    } catch (...) {
        emitError("Unknown error setting up defaults");
    }
}

bool ConfigManager::validateValue(const QString& key, const QVariant& value) const noexcept
{
    // Basic validation - can be extended based on requirements
    if (key.isEmpty() || !value.isValid()) {
        return false;
    }

    // Specific validations for known keys
    if (key == "media/volume") {
        bool ok;
        const double vol = value.toDouble(&ok);
        return ok && vol >= 0.0 && vol <= 1.0;
    }

    if (key == "files/maxRecentFiles") {
        bool ok;
        const int max = value.toInt(&ok);
        return ok && max >= 0 && max <= 100;
    }

    if (key == "performance/bufferSize") {
        bool ok;
        const int size = value.toInt(&ok);
        return ok && size >= 1024 && size <= 65536;
    }

    return true; // Default: accept all other values
}

void ConfigManager::emitError(const QString& error) const noexcept
{
    try {
        // Use QueuedConnection to prevent issues if called from different threads
        QMetaObject::invokeMethod(const_cast<ConfigManager*>(this), 
                                  "errorOccurred", 
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, error));
    } catch (...) {
        // Last resort - log to debug output
        qWarning() << "ConfigManager error:" << error;
    }
}

QJsonObject ConfigManager::variantMapToJson(const QVariantMap& map) noexcept
{
    try {
        QJsonObject obj;
        for (auto it = map.begin(); it != map.end(); ++it) {
            const QJsonValue value = QJsonValue::fromVariant(it.value());
            if (!value.isUndefined()) {
                obj[it.key()] = value;
            }
        }
        return obj;
    } catch (...) {
        return {};
    }
}

QVariantMap ConfigManager::jsonToVariantMap(const QJsonObject& json) noexcept
{
    try {
        QVariantMap map;
        for (auto it = json.begin(); it != json.end(); ++it) {
            const QVariant variant = it.value().toVariant();
            if (variant.isValid()) {
                map[it.key()] = variant;
            }
        }
        return map;
    } catch (...) {
        return {};
    }
}

} // namespace DarkPlay::Core
