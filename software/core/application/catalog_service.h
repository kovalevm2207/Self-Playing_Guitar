#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <string>

#include "application/ports.h"

namespace guitar {

enum class CatalogChange { Data, Favorites, Status };

class CatalogService {
public:
    using Subscription = std::size_t;
    using Observer = std::function<void(CatalogChange)>;

    CatalogService(CatalogRepository& repository, FavoriteStore& favorites);
    CatalogService(const CatalogService&) = delete;
    CatalogService& operator=(const CatalogService&) = delete;

    // Success describes the catalog; optional favorites report a separate error.
    bool reload();
    bool toggleFavorite(const std::string& arrangementId);
    bool isFavorite(const std::string& arrangementId) const;
    const CatalogData& data() const { return m_data; }
    const std::string& catalogError() const { return m_catalogError; }
    const std::string& favoritesError() const { return m_favoritesError; }

    // Delivered synchronously after committing state, on the application thread.
    // Unsubscribe before the observer is destroyed; this service must outlive it.
    Subscription subscribe(Observer observer);
    void unsubscribe(Subscription subscription);

    const Song* song(const std::string& id) const;
    const CatalogArrangement* arrangement(const std::string& id) const;

private:
    void notify(CatalogChange change);

    CatalogRepository& m_repository;
    FavoriteStore& m_favoriteStore;
    CatalogData m_data;
    std::set<std::string> m_favorites;
    std::string m_catalogError;
    std::string m_favoritesError;
    bool m_favoritesLoaded = false;
    std::map<Subscription, Observer> m_observers;
    Subscription m_nextSubscription = 0;
};

} // namespace guitar
