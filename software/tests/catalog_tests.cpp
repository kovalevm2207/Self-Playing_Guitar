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
    assert(catalog.error().empty());
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
    assert(!catalog.error().empty());
    favorites.failSave = false;
    assert(catalog.toggleFavorite("one"));
    assert(catalog.error().empty());
    assert(!catalog.isFavorite("one"));

    repository.data.arrangements[0].music.songId = "unknown";
    assert(!catalog.reload());
    assert(catalog.arrangement("one")->music.songId == "song");
    repository.data = test::sampleCatalog();
    repository.data.arrangements.push_back(repository.data.arrangements.front());
    assert(!catalog.reload());
    assert(catalog.data().arrangements.size() == 2);
}

int main() {
    checkMusic();
    checkCatalog();
    std::cout << "PASS: music, tempo, long arrangements, references, transactional "
                 "favorites\n";
}
