#pragma once

#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "domain/music.h"

namespace guitar {

struct CatalogArtwork {
    std::string color;
    std::string mark;
};

struct CatalogArrangement {
    Arrangement music;
    CatalogArtwork artwork;
};

struct CatalogData {
    std::vector<Song> songs;
    std::vector<CatalogArrangement> arrangements;
};

class CatalogRepository {
public:
    virtual ~CatalogRepository() = default;
    virtual CatalogData load() = 0;
};

class FavoriteStore {
public:
    virtual ~FavoriteStore() = default;
    virtual std::set<std::string> load() = 0;
    virtual void save(const std::set<std::string>& ids) = 0;
};

// Callbacks are delivered on the application's thread. The session owns the
// subscription, and the transport must outlive it.
struct DeviceEvents {
    std::function<void()> ready;
    std::function<void(std::string)> received;
    std::function<void(std::string)> failed;
    std::function<void()> timedOut;
};

class DeviceTransport {
public:
    virtual ~DeviceTransport() = default;
    virtual void setEvents(DeviceEvents events) = 0;
    virtual std::vector<std::string> ports() const = 0;
    virtual void open(const std::string& port) = 0;
    virtual void close() = 0;
    virtual bool write(std::string_view bytes) = 0;
    virtual void startTimeout(int milliseconds) = 0;
    virtual void stopTimeout() = 0;
};

} // namespace guitar
