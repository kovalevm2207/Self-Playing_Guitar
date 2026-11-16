#include "application/catalog_service.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace guitar {

CatalogService::CatalogService(CatalogRepository& repository,
                               FavoriteStore& favorites)
    : m_repository(repository), m_favoriteStore(favorites) {
    reload();
}

bool CatalogService::reload() {
    try {
        auto data = m_repository.load();
        std::set<std::string> songIds;
        std::set<std::string> arrangementIds;
        for (const auto& song : data.songs) {
            if (song.id.empty() || song.title.empty() ||
                !songIds.insert(song.id).second) {
                throw std::invalid_argument("Invalid or duplicate song ID");
            }
        }
        for (const auto& entry : data.arrangements) {
            if (!valid(entry.music) || !songIds.count(entry.music.songId) ||
                !arrangementIds.insert(entry.music.id).second) {
                throw std::invalid_argument("Invalid arrangement or song reference");
            }
        }

        auto favorites = m_favoriteStore.load();
        m_data = std::move(data);
        m_favorites = std::move(favorites);
        m_error.clear();
        return true;
    } catch (const std::exception& error) {
        m_error = error.what();
        return false;
    }
}

const Song* CatalogService::song(const std::string& id) const {
    const auto found = std::find_if(m_data.songs.begin(),
                                    m_data.songs.end(),
                                    [&](const Song& song) { return song.id == id; });
    return found == m_data.songs.end() ? nullptr : &*found;
}

const CatalogArrangement* CatalogService::arrangement(const std::string& id) const {
    const auto found = std::find_if(
        m_data.arrangements.begin(),
        m_data.arrangements.end(),
        [&](const CatalogArrangement& entry) { return entry.music.id == id; });
    return found == m_data.arrangements.end() ? nullptr : &*found;
}

bool CatalogService::isFavorite(const std::string& arrangementId) const {
    return m_favorites.count(arrangementId) != 0;
}

bool CatalogService::toggleFavorite(const std::string& arrangementId) {
    if (!arrangement(arrangementId)) {
        return false;
    }

    auto favorites = m_favorites;
    if (!favorites.erase(arrangementId)) {
        favorites.insert(arrangementId);
    }
    try {
        m_favoriteStore.save(favorites);
        m_favorites = std::move(favorites);
        m_error.clear();
        return true;
    } catch (const std::exception& error) {
        m_error = error.what();
        return false;
    }
}

} // namespace guitar
