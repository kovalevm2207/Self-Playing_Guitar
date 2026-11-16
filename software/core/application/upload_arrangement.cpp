#include "application/upload_arrangement.h"

#include <stdexcept>

namespace guitar {

UploadArrangement::UploadArrangement(const CatalogService& catalog,
                                     TransferSession& session)
    : m_catalog(catalog), m_session(session) {}

void UploadArrangement::execute(const std::string& arrangementId, int tempo) {
    const auto* entry = m_catalog.arrangement(arrangementId);
    if (!entry) {
        m_session.reportInvalidScore("Unknown arrangement");
        return;
    }
    try {
        const auto* song = m_catalog.song(entry->music.songId);
        m_session.upload(prepareScore(entry->music, song->title, tempo));
    } catch (const std::invalid_argument& error) {
        m_session.reportInvalidScore(error.what());
    }
}

} // namespace guitar
