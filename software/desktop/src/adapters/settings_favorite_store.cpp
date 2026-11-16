#include "adapters/settings_favorite_store.h"

#include <stdexcept>

namespace guitar {

SettingsFavoriteStore::SettingsFavoriteStore(const QString& path) {
    if (path.isEmpty()) {
        m_settings = std::make_unique<QSettings>();
    } else {
        m_settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
    }
}

std::set<std::string> SettingsFavoriteStore::load() {
    std::set<std::string> result;
    for (const auto& id : m_settings->value("favorites").toStringList()) {
        result.insert(id.toStdString());
    }
    if (m_settings->status() != QSettings::NoError) {
        throw std::runtime_error("Cannot read favorites");
    }
    return result;
}

void SettingsFavoriteStore::save(const std::set<std::string>& ids) {
    QStringList values;
    for (const auto& id : ids) {
        values.append(QString::fromStdString(id));
    }
    m_settings->setValue("favorites", values);
    m_settings->sync();
    if (m_settings->status() != QSettings::NoError) {
        throw std::runtime_error("Cannot save favorites");
    }
}

} // namespace guitar
