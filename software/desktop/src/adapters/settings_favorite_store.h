#pragma once

#include <QSettings>
#include <memory>

#include "application/ports.h"

namespace guitar {

class SettingsFavoriteStore : public FavoriteStore {
public:
    explicit SettingsFavoriteStore(const QString& path = {});
    std::set<std::string> load() override;
    void save(const std::set<std::string>& ids) override;

private:
    std::unique_ptr<QSettings> m_settings;
};

} // namespace guitar
