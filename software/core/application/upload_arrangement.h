#pragma once

#include "application/catalog_service.h"
#include "application/transfer_session.h"

namespace guitar {

class UploadArrangement {
public:
    UploadArrangement(const CatalogService& catalog, TransferSession& session);
    void execute(const std::string& arrangementId, int tempo);

private:
    const CatalogService& m_catalog;
    TransferSession& m_session;
};

} // namespace guitar
