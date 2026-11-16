#pragma once

#include <QString>

#include "application/ports.h"

namespace guitar {

class FileFavoriteStore : public FavoriteStore {
public:
    explicit FileFavoriteStore(const QString& path = {});
    std::set<std::string> load() override;
    void save(const std::set<std::string>& ids) override;

private:
    QString m_path;
    bool m_readLegacySettings;
};

} // namespace guitar
