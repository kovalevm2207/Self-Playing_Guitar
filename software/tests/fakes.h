#pragma once

#include <stdexcept>
#include <utility>

#include "application/ports.h"

namespace guitar::test {

inline CatalogData sampleCatalog() {
    return {{{"song", "Test song", "Author"}},
            {{{"one", "song", "One string", "", 120, {{1, 0, 500}, {1, 3, 250}}},
              {"#53634b", "♫"}},
             {{"two", "song", "Two strings", "", 100, {{2, 1, 600}, {1, 0, 600}}},
              {"#65563e", "✦"}}}};
}

struct MemoryCatalog : CatalogRepository {
    CatalogData data = sampleCatalog();
    CatalogData load() override { return data; }
};

struct MemoryFavorites : FavoriteStore {
    std::set<std::string> ids;
    bool failSave = false;
    std::set<std::string> load() override { return ids; }
    void save(const std::set<std::string>& values) override {
        if (failSave) {
            throw std::runtime_error("Storage failure");
        }
        ids = values;
    }
};

struct FakeTransport : DeviceTransport {
    DeviceEvents events;
    std::vector<std::string> availablePorts{"TEST"};
    std::vector<std::string> sent;
    std::string openedPort;
    bool opened = false;
    bool failWrite = false;
    int timeoutMs = 0;

    void setEvents(DeviceEvents value) override { events = std::move(value); }
    std::vector<std::string> ports() const override { return availablePorts; }
    void open(const std::string& port) override {
        opened = true;
        openedPort = port;
    }
    void close() override {
        opened = false;
        timeoutMs = 0;
    }
    bool write(std::string_view bytes) override {
        if (failWrite) {
            return false;
        }
        sent.emplace_back(bytes);
        return true;
    }
    void startTimeout(int value) override { timeoutMs = value; }
    void stopTimeout() override { timeoutMs = 0; }
    void receive(const std::string& bytes) {
        if (events.received) {
            events.received(bytes);
        }
    }
    void ready() {
        if (events.ready) {
            events.ready();
        }
    }
};

} // namespace guitar::test
