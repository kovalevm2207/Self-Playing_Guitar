#undef NDEBUG
#include <cassert>
#include <iostream>

#include "application/catalog_service.h"
#include "domain/music.h"
#include "fakes.h"

using namespace guitar;

void checkMusic() {
    auto arrangement = test::sampleCatalog().arrangements[1].music;
    assert(valid(arrangement));
    assert((usedStrings(arrangement) == std::vector<int>{1, 2}));
    assert(durationMs(arrangement) == 1200);
    const auto score = prepareScore(arrangement, "Song", 200);
    assert(score.notes.size() == 2);
    assert(score.notes[0].duration == 300);
    assert(arrangement.notes[0].duration == 600);

    arrangement.notes.assign(200, {1, 0, 500});
    assert(valid(arrangement)); // The catalog is not constrained by device RAM.
    assert(prepareScore(arrangement, "Long song", 100).notes.size() == 200);

    bool rejected = false;
    try {
        prepareScore(arrangement, "Song", 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    arrangement.notes = {{1, 0, 50}};
    rejected = false;
    try {
        prepareScore(arrangement, "Song", 220);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

void checkCatalog() {
    test::MemoryCatalog repository;
    test::MemoryFavorites favorites;
    CatalogService catalog(repository, favorites);
    assert(catalog.catalogError().empty());
    assert(catalog.favoritesError().empty());
    assert(catalog.data().songs.size() == 1);
    assert(catalog.data().arrangements.size() == 2);
    assert(catalog.song(catalog.arrangement("two")->music.songId)->title ==
           "Test song");
    assert(!catalog.arrangement("missing"));
    assert(!catalog.toggleFavorite("missing"));
    assert(catalog.toggleFavorite("one"));
    assert(catalog.isFavorite("one"));
    assert(favorites.ids.count("one"));

    favorites.failSave = true;
    assert(!catalog.toggleFavorite("one"));
    assert(
        catalog.isFavorite("one")); // Failed persistence cannot change the UI state.
    assert(!catalog.favoritesError().empty());
    assert(catalog.catalogError().empty());
    favorites.failSave = false;
    assert(catalog.toggleFavorite("one"));
    assert(catalog.favoritesError().empty());
    assert(!catalog.isFavorite("one"));

    repository.data.arrangements[0].music.songId = "unknown";
    assert(!catalog.reload());
    assert(catalog.arrangement("one")->music.songId == "song");
    repository.data = test::sampleCatalog();
    repository.data.arrangements.push_back(repository.data.arrangements.front());
    assert(!catalog.reload());
    assert(catalog.data().arrangements.size() == 2);
    const auto error = catalog.catalogError();
    assert(!error.empty());
    assert(catalog.toggleFavorite("one"));
    assert(catalog.catalogError() == error);
    repository.data = test::sampleCatalog();
    assert(catalog.reload());
    assert(catalog.catalogError().empty());
}

void checkUnavailableFavorites() {
    test::MemoryCatalog repository;
    test::MemoryFavorites favorites;
    favorites.ids = {"two"};
    favorites.failLoad = true;
    CatalogService catalog(repository, favorites);
    assert(catalog.catalogError().empty());
    assert(!catalog.favoritesError().empty());
    assert(catalog.data().songs.size() == 1);
    assert(catalog.data().arrangements.size() == 2);

    // Unread preferences cannot be replaced with an empty fallback snapshot.
    assert(!catalog.toggleFavorite("one"));
    assert((favorites.ids == std::set<std::string>{"two"}));
    favorites.failLoad = false;
    assert(catalog.toggleFavorite("one"));
    assert(catalog.isFavorite("one"));
    assert(catalog.isFavorite("two"));
    assert(catalog.favoritesError().empty());

    favorites.failLoad = true;
    repository.data.songs[0].title = "Updated song";
    assert(catalog.reload());
    assert(catalog.song("song")->title == "Updated song");
    assert(catalog.catalogError().empty());
    assert(!catalog.favoritesError().empty());
    assert(catalog.isFavorite("one"));
    assert(catalog.isFavorite("two"));
}

void checkCatalogNotifications() {
    test::MemoryCatalog repository;
    test::MemoryFavorites favorites;
    CatalogService catalog(repository, favorites);
    std::vector<CatalogChange> changes;
    const auto subscription =
        catalog.subscribe([&](CatalogChange change) { changes.push_back(change); });
    assert(catalog.reload());
    assert(catalog.toggleFavorite("one"));
    favorites.failSave = true;
    assert(!catalog.toggleFavorite("one"));
    repository.data.songs.clear();
    assert(!catalog.reload());
    assert((changes == std::vector<CatalogChange>{CatalogChange::Data,
                                                  CatalogChange::Favorites,
                                                  CatalogChange::Status,
                                                  CatalogChange::Status}));
    catalog.unsubscribe(subscription);
    assert(!catalog.reload());
    assert(changes.size() == 4);

    CatalogService::Subscription first = 0;
    first = catalog.subscribe([&](CatalogChange) { catalog.unsubscribe(first); });
    const auto second =
        catalog.subscribe([&](CatalogChange change) { changes.push_back(change); });
    assert(!catalog.reload());
    assert(!catalog.reload());
    assert(changes.size() == 6);
    catalog.unsubscribe(second);
}

int main() {
    checkMusic();
    checkCatalog();
    checkUnavailableFavorites();
    checkCatalogNotifications();
    std::cout << "PASS: music, tempo, long arrangements, references, transactional "
                 "favorites, independent errors, catalog notifications\n";
}
