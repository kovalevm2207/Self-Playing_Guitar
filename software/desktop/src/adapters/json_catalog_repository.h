#pragma once

#include <QString>

#include "application/ports.h"

namespace guitar {

class JsonCatalogRepository : public CatalogRepository {
public:
    explicit JsonCatalogRepository(QString path);
    CatalogData load() override;

private:
    QString m_path;
};

} // namespace guitar
