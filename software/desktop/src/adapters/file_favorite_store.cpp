#include "adapters/file_favorite_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <stdexcept>

namespace guitar {

FileFavoriteStore::FileFavoriteStore(const QString& path)
    : m_path(path), m_readLegacySettings(path.isEmpty()) {
    if (m_readLegacySettings) {
        const auto directory =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        if (!directory.isEmpty()) {
            m_path = QDir(directory).filePath("favorites.json");
        }
    }
}

std::set<std::string> FileFavoriteStore::load() {
    if (m_path.isEmpty()) {
        throw std::runtime_error("Favorites directory is unavailable");
    }

    std::set<std::string> result;
    if (!QFileInfo::exists(m_path)) {
        // Read the previous format only until the first successful JSON save.
        // This QSettings instance never receives writes, including on destruction.
        if (m_readLegacySettings) {
            QSettings settings;
            for (const auto& id : settings.value("favorites").toStringList()) {
                if (!id.isEmpty()) {
                    result.insert(id.toStdString());
                }
            }
            if (settings.status() != QSettings::NoError) {
                throw std::runtime_error("Cannot read legacy favorites");
            }
        }
        return result;
    }

    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Cannot read favorites");
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto root = document.object();
    if (file.error() != QFileDevice::NoError || !document.isObject() ||
        root.value("version") != QJsonValue(1) ||
        !root.value("favorites").isArray()) {
        throw std::runtime_error("Invalid favorites file");
    }
    for (const auto& value : root.value("favorites").toArray()) {
        if (!value.isString() || value.toString().isEmpty()) {
            throw std::runtime_error("Invalid favorite ID");
        }
        result.insert(value.toString().toStdString());
    }
    return result;
}

void FileFavoriteStore::save(const std::set<std::string>& ids) {
    QJsonArray values;
    for (const auto& id : ids) {
        if (id.empty()) {
            throw std::invalid_argument("Empty favorite ID");
        }
        values.append(QString::fromStdString(id));
    }
    const auto bytes =
        QJsonDocument(QJsonObject{{"version", 1}, {"favorites", values}}).toJson();

    if (m_path.isEmpty() || !QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        throw std::runtime_error("Cannot create favorites directory");
    }
    QSaveFile file(m_path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
        !file.commit()) {
        throw std::runtime_error("Cannot save favorites");
    }
}

} // namespace guitar
