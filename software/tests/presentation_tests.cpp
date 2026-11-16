#undef NDEBUG
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

#include "adapters/json_catalog_repository.h"
#include "adapters/qt_serial_transport.h"
#include "adapters/settings_favorite_store.h"
#include "fakes.h"
#include "presentation/catalog_view_model.h"
#include "presentation/transfer_view_model.h"

using namespace guitar;

void checkStorage() {
    QTemporaryDir directory;
    assert(directory.isValid());
    const auto path = directory.filePath("favorites.ini");
    {
        SettingsFavoriteStore store(path);
        store.save({"ode-one", "twinkle"});
    }
    SettingsFavoriteStore restored(path);
    assert((restored.load() == std::set<std::string>{"ode-one", "twinkle"}));

    QFile input(GUITAR_CATALOG_FILE);
    assert(input.open(QIODevice::ReadOnly));
    auto root = QJsonDocument::fromJson(input.readAll()).object();
    auto arrangements = root["arrangements"].toArray();
    auto arrangement = arrangements[0].toObject();
    auto notes = arrangement["notes"].toArray();
    auto note = notes[0].toArray();
    note[0] = 1.5;
    notes[0] = note;
    arrangement["notes"] = notes;
    arrangements[0] = arrangement;
    root["arrangements"] = arrangements;
    const auto invalidPath = directory.filePath("invalid.json");
    QFile output(invalidPath);
    assert(output.open(QIODevice::WriteOnly));
    output.write(QJsonDocument(root).toJson());
    output.close();
    JsonCatalogRepository invalid(invalidPath);
    bool rejected = false;
    try {
        invalid.load();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

void checkMissingSerialPort() {
    QtSerialTransport transport;
    TransferSession session(transport);
    session.connectPort("__selfguitar_missing_test_port__");
    assert(session.state().phase == TransferPhase::Disconnected);
    assert(session.state().error == TransferError::Connection);
    assert(!session.state().details.empty());
}

void checkPresentation() {
    JsonCatalogRepository repository(GUITAR_CATALOG_FILE);
    test::MemoryFavorites favorites;
    CatalogService catalog(repository, favorites);
    assert(catalog.error().empty());
    assert(catalog.data().songs.size() == 6);
    assert(catalog.data().arrangements.size() == 6);

    test::FakeTransport transport;
    TransferSession session(transport);
    UploadArrangement upload(catalog, session);
    CatalogViewModel catalogView(catalog);
    TransferViewModel transferView(session, transport, upload);
    assert(catalogView.selectedArrangementId() == "ode-one");
    assert(catalogView.selectedArrangement()["stringCount"].toInt() == 1);
    assert(catalogView.selectedArrangement()["noteCount"].toInt() == 15);

    QQmlApplicationEngine engine;
    QStringList warnings;
    QObject::connect(&engine,
                     &QQmlEngine::warnings,
                     &engine,
                     [&](const QList<QQmlError>& errors) {
                         for (const auto& error : errors) {
                             warnings.append(error.toString());
                         }
                     });
    engine.rootContext()->setContextProperty("catalog", &catalogView);
    engine.rootContext()->setContextProperty("transfer", &transferView);
    engine.load(QUrl::fromLocalFile(GUITAR_QML_FILE));
    assert(!engine.rootObjects().isEmpty());
    auto* window = engine.rootObjects().front();
    auto* search = window->findChild<QObject*>("catalogSearch");
    auto* filter = window->findChild<QObject*>("stringFilter");
    auto* tempo = window->findChild<QObject*>("playbackTempo");
    assert(search && filter && tempo);

    search->setProperty("text", "БЕТХОВЕН");
    QCoreApplication::processEvents();
    assert(catalogView.arrangements().size() == 1);
    search->setProperty("text", "ничего такого нет");
    QCoreApplication::processEvents();
    assert(catalogView.arrangements().empty());
    search->setProperty("text", "");
    filter->setProperty("currentIndex", 2);
    QCoreApplication::processEvents();
    assert(catalogView.arrangements().size() == 2);

    catalogView.selectArrangement("twinkle");
    assert(tempo->property("value").toInt() == 90);
    tempo->setProperty("value", 105);
    catalogView.toggleFavorite("twinkle");
    QCoreApplication::processEvents();
    assert(tempo->property("value").toInt() == 105);
    window->setProperty("page", 1);
    QCoreApplication::processEvents();
    assert(catalogView.arrangements().size() == 1);
    assert(catalogView.arrangements().front().toMap()["id"] == "twinkle");
    catalogView.selectArrangement("missing");
    assert(catalogView.selectedArrangementId() == "twinkle");
    catalogView.toggleFavorite("twinkle");
    assert(catalogView.arrangements().empty());
    assert(catalogView.selectedArrangementId() == "twinkle");

    int stateChanges = 0;
    QObject::connect(&transferView, &TransferViewModel::stateChanged, &engine, [&] {
        ++stateChanges;
    });
    transferView.connectPort("TEST");
    assert(transferView.busy());
    transport.ready();
    transport.receive("READY 1 96 DRYRUN\n");
    assert(transferView.connected() && !transferView.busy());
    assert(QMetaObject::invokeMethod(
        &transferView, "upload", Q_ARG(QString, "twinkle"), Q_ARG(int, 90)));
    assert(transferView.busy());
    transport.receive("OK BEGIN\n");
    const auto count = catalog.arrangement("twinkle")->music.notes.size();
    for (size_t index = 0; index < count; ++index) {
        transport.receive("OK " + std::to_string(index) + "\n");
    }
    transport.receive("OK END\n");
    QCoreApplication::processEvents();
    assert(transferView.progress() == 1 && !transferView.busy());
    assert(!transferView.loadedTitle().isEmpty());
    assert(stateChanges > 0);

    for (const auto& warning : warnings) {
        std::cerr << warning.toStdString() << '\n';
    }
    assert(warnings.empty());
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    checkStorage();
    checkMissingSerialPort();
    checkPresentation();
    std::cout << "PASS: JSON validation, persisted favorites, QML filters, "
                 "selection, upload bindings\n";
}
