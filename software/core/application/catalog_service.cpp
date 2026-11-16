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
    CatalogData data;
    try {
        data = m_repository.load();
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
    } catch (const std::exception& error) {
        m_catalogError = error.what();
        notify(CatalogChange::Status);
        return false;
    }

    try {
        auto favorites = m_favoriteStore.load();
        m_favorites = std::move(favorites);
        m_favoritesLoaded = true;
        m_favoritesError.clear();
    } catch (const std::exception& error) {
        m_favoritesError = error.what();
    }

    m_data = std::move(data);
    m_catalogError.clear();
    notify(CatalogChange::Data);
    return true;
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

    try {
        // Do not overwrite unread preferences with an empty fallback snapshot.
        auto favorites = m_favoritesLoaded ? m_favorites : m_favoriteStore.load();
        if (!favorites.erase(arrangementId)) {
            favorites.insert(arrangementId);
        }
        m_favoriteStore.save(favorites);
        m_favorites = std::move(favorites);
        m_favoritesLoaded = true;
        m_favoritesError.clear();
    } catch (const std::exception& error) {
        m_favoritesError = error.what();
        notify(CatalogChange::Status);
        return false;
    }
    notify(CatalogChange::Favorites);
    return true;
}

CatalogService::Subscription CatalogService::subscribe(Observer observer) {
    const auto subscription = ++m_nextSubscription;
    m_observers.emplace(subscription, std::move(observer));
    return subscription;
}

void CatalogService::unsubscribe(Subscription subscription) {
    m_observers.erase(subscription);
}

void CatalogService::notify(CatalogChange change) {
    // Observers may unsubscribe themselves or another observer during delivery.
    const auto observers = m_observers;
    for (const auto& [subscription, observer] : observers) {
        if (m_observers.count(subscription) && observer) {
            observer(change);
        }
    }
}

} // namespace guitar
