#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "application/catalog_service.h"

namespace guitar {

class CatalogViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(
        QVariantList arrangements READ arrangements NOTIFY arrangementsChanged)
    Q_PROPERTY(QString selectedArrangementId READ selectedArrangementId NOTIFY
                   selectionChanged)
    Q_PROPERTY(QVariantMap selectedArrangement READ selectedArrangement NOTIFY
                   selectedArrangementChanged)
    Q_PROPERTY(
        QString searchText READ searchText WRITE setSearchText NOTIFY filtersChanged)
    Q_PROPERTY(
        int maxStrings READ maxStrings WRITE setMaxStrings NOTIFY filtersChanged)
    Q_PROPERTY(bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY
                   filtersChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit CatalogViewModel(CatalogService& catalog, QObject* parent = nullptr);

    QVariantList arrangements() const;
    QVariantMap selectedArrangement() const;
    QString selectedArrangementId() const { return m_selectedId; }
    QString searchText() const { return m_searchText; }
    int maxStrings() const { return m_maxStrings; }
    bool favoritesOnly() const { return m_favoritesOnly; }
    QString error() const;

    void setSearchText(const QString& text);
    void setMaxStrings(int count);
    void setFavoritesOnly(bool enabled);
    Q_INVOKABLE void selectArrangement(const QString& id);
    Q_INVOKABLE void toggleFavorite(const QString& id);

signals:
    void arrangementsChanged();
    void selectionChanged();
    void selectedArrangementChanged();
    void filtersChanged();
    void errorChanged();

private:
    QVariantMap describe(const CatalogArrangement& entry) const;

    CatalogService& m_catalog;
    QString m_selectedId;
    QString m_searchText;
    int m_maxStrings = 0;
    bool m_favoritesOnly = false;
};

} // namespace guitar
