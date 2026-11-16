#include "presentation/catalog_view_model.h"

namespace guitar {

CatalogViewModel::CatalogViewModel(CatalogService& catalog, QObject* parent)
    : QObject(parent), m_catalog(catalog) {
    if (!catalog.data().arrangements.empty()) {
        m_selectedId =
            QString::fromStdString(catalog.data().arrangements.front().music.id);
    }
    m_subscription = m_catalog.subscribe(
        [this](CatalogChange change) { catalogChanged(change); });
}

CatalogViewModel::~CatalogViewModel() {
    m_catalog.unsubscribe(m_subscription);
}

void CatalogViewModel::catalogChanged(CatalogChange change) {
    if (change == CatalogChange::Data) {
        const auto previousId = m_selectedId;
        if (!m_catalog.arrangement(m_selectedId.toStdString())) {
            m_selectedId = m_catalog.data().arrangements.empty()
                               ? QString{}
                               : QString::fromStdString(
                                     m_catalog.data().arrangements.front().music.id);
        }
        emit selectedArrangementChanged();
        if (previousId != m_selectedId) {
            emit selectionChanged();
        }
    } else if (change == CatalogChange::Favorites) {
        emit selectedArrangementChanged();
    }
    if (change != CatalogChange::Status) {
        emit arrangementsChanged();
    }
    emit errorChanged();
}

QVariantMap CatalogViewModel::describe(const CatalogArrangement& entry) const {
    const auto& arrangement = entry.music;
    const auto* song = m_catalog.song(arrangement.songId);
    QVariantList strings;
    for (const auto number : usedStrings(arrangement)) {
        strings.append(number);
    }
    return {{"id", QString::fromStdString(arrangement.id)},
            {"songId", QString::fromStdString(arrangement.songId)},
            {"title", QString::fromStdString(song->title)},
            {"author", QString::fromStdString(song->author)},
            {"arrangement", QString::fromStdString(arrangement.label)},
            {"description", QString::fromStdString(arrangement.description)},
            {"bpm", arrangement.tempo},
            {"strings", strings},
            {"stringCount", strings.size()},
            {"noteCount", static_cast<qulonglong>(arrangement.notes.size())},
            {"durationMs", static_cast<qulonglong>(durationMs(arrangement))},
            {"color", QString::fromStdString(entry.artwork.color)},
            {"mark", QString::fromStdString(entry.artwork.mark)},
            {"favorite", m_catalog.isFavorite(arrangement.id)}};
}

QVariantList CatalogViewModel::arrangements() const {
    QVariantList result;
    for (const auto& entry : m_catalog.data().arrangements) {
        const auto* song = m_catalog.song(entry.music.songId);
        if (m_favoritesOnly && !m_catalog.isFavorite(entry.music.id)) {
            continue;
        }
        if (m_maxStrings &&
            usedStrings(entry.music).size() > static_cast<size_t>(m_maxStrings)) {
            continue;
        }
        const auto searchable =
            QString::fromStdString(song->title + " " + song->author);
        if (!searchable.contains(m_searchText, Qt::CaseInsensitive)) {
            continue;
        }
        result.append(describe(entry));
    }
    return result;
}

QVariantMap CatalogViewModel::selectedArrangement() const {
    const auto* entry = m_catalog.arrangement(m_selectedId.toStdString());
    return entry ? describe(*entry) : QVariantMap{};
}

QString CatalogViewModel::error() const {
    if (!m_catalog.catalogError().empty()) {
        return "Ошибка каталога: " +
               QString::fromStdString(m_catalog.catalogError());
    }
    if (!m_catalog.favoritesError().empty()) {
        return "Ошибка избранного: " +
               QString::fromStdString(m_catalog.favoritesError());
    }
    return {};
}

void CatalogViewModel::selectArrangement(const QString& id) {
    if (id == m_selectedId || !m_catalog.arrangement(id.toStdString())) {
        return;
    }
    m_selectedId = id;
    emit selectedArrangementChanged();
    emit selectionChanged();
}

void CatalogViewModel::toggleFavorite(const QString& id) {
    m_catalog.toggleFavorite(id.toStdString());
}

bool CatalogViewModel::reload() {
    return m_catalog.reload();
}

void CatalogViewModel::setSearchText(const QString& text) {
    if (text != m_searchText) {
        m_searchText = text;
        emit filtersChanged();
        emit arrangementsChanged();
    }
}

void CatalogViewModel::setMaxStrings(int count) {
    if (count >= 0 && count <= 6 && count != m_maxStrings) {
        m_maxStrings = count;
        emit filtersChanged();
        emit arrangementsChanged();
    }
}

void CatalogViewModel::setFavoritesOnly(bool enabled) {
    if (enabled != m_favoritesOnly) {
        m_favoritesOnly = enabled;
        emit filtersChanged();
        emit arrangementsChanged();
    }
}

} // namespace guitar
