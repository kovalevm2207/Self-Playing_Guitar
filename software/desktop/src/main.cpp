#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include "adapters/file_favorite_store.h"
#include "adapters/json_catalog_repository.h"
#include "adapters/qt_serial_transport.h"
#include "presentation/catalog_view_model.h"
#include "presentation/transfer_view_model.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("SelfGuitar");
    app.setApplicationName("SelfGuitar");
    QQuickStyle::setStyle("Basic");

    guitar::JsonCatalogRepository repository(":/data/songs.json");
    guitar::FileFavoriteStore favorites;
    guitar::CatalogService catalog(repository, favorites);
    guitar::QtSerialTransport transport;
    guitar::TransferSession session(transport);
    guitar::UploadArrangement upload(catalog, session);
    guitar::CatalogViewModel catalogViewModel(catalog);
    guitar::TransferViewModel transferViewModel(session, transport, upload);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("catalog", &catalogViewModel);
    engine.rootContext()->setContextProperty("transfer", &transferViewModel);
    engine.loadFromModule("Guitar", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    const auto arguments = app.arguments();
    const int shot = arguments.indexOf("--screenshot");
    if (shot >= 0 && shot + 1 < arguments.size()) {
        const auto path = arguments.at(shot + 1);
        QTimer::singleShot(1200, &app, [&app, &engine, path] {
            auto window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            app.exit(window && window->grabWindow().save(path) ? 0 : 2);
        });
    }

    return app.exec();
}
