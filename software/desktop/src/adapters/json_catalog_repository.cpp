#include "adapters/json_catalog_repository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace guitar {
namespace {

std::string text(const QJsonObject& object, const char* key) {
    const auto value = object.value(QLatin1String(key));
    if (!value.isString()) {
        throw std::invalid_argument(std::string("Expected string: ") + key);
    }
    return value.toString().toStdString();
}

int integer(const QJsonValue& value, int minimum, int maximum) {
    if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
        value.toDouble() != std::floor(value.toDouble()) ||
        value.toDouble() < minimum || value.toDouble() > maximum) {
        throw std::invalid_argument("Invalid numeric field in catalog");
    }
    return value.toInt();
}

} // namespace

JsonCatalogRepository::JsonCatalogRepository(QString path)
    : m_path(std::move(path)) {}

CatalogData JsonCatalogRepository::load() {
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Cannot open catalog: " + m_path.toStdString());
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    const auto root = document.object();
    if (error.error != QJsonParseError::NoError || !document.isObject() ||
        !root.value("songs").isArray() || !root.value("arrangements").isArray()) {
        throw std::invalid_argument("Expected songs and arrangements arrays");
    }

    CatalogData result;
    for (const auto& value : root.value("songs").toArray()) {
        const auto song = value.toObject();
        result.songs.push_back(
            {text(song, "id"), text(song, "title"), text(song, "author")});
    }
    for (const auto& value : root.value("arrangements").toArray()) {
        const auto object = value.toObject();
        CatalogArrangement entry;
        auto& arrangement = entry.music;
        arrangement.id = text(object, "id");
        arrangement.songId = text(object, "songId");
        arrangement.label = text(object, "label");
        arrangement.description = text(object, "description");
        arrangement.tempo = integer(object.value("bpm"), 40, 220);
        if (!object.value("notes").isArray()) {
            throw std::invalid_argument("Expected a notes array");
        }
        for (const auto& event : object.value("notes").toArray()) {
            const auto fields = event.toArray();
            if (!event.isArray() || fields.size() != 3) {
                throw std::invalid_argument("Expected [string, fret, durationMs]");
            }
            arrangement.notes.push_back(
                {static_cast<uint8_t>(integer(fields[0], 1, 6)),
                 static_cast<uint8_t>(integer(fields[1], 0, 24)),
                 static_cast<uint16_t>(integer(fields[2], 50, 10000))});
        }
        entry.artwork = {text(object, "color"), text(object, "mark")};
        result.arrangements.push_back(std::move(entry));
    }
    return result;
}

} // namespace guitar
