#pragma once

#include <set>
#include <string>

#include "application/ports.h"

namespace guitar {

class CatalogService {
public:
    CatalogService(CatalogRepository& repository, FavoriteStore& favorites);

    bool reload();
    bool toggleFavorite(const std::string& arrangementId);
    bool isFavorite(const std::string& arrangementId) const;
    const CatalogData& data() const { return m_data; }
    const std::string& error() const { return m_error; }
    const Song* song(const std::string& id) const;
    const CatalogArrangement* arrangement(const std::string& id) const;

private:
    CatalogRepository& m_repository;
    FavoriteStore& m_favoriteStore;
    CatalogData m_data;
    std::set<std::string> m_favorites;
    std::string m_error;
};

} // namespace guitar
