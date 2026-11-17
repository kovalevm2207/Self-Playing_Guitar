#undef NDEBUG
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

#include "adapters/file_favorite_store.h"
#include "adapters/json_catalog_repository.h"
#include "adapters/qt_serial_transport.h"
#include "fakes.h"
#include "presentation/catalog_view_model.h"
#include "presentation/transfer_view_model.h"

using namespace guitar;

void checkFavoriteStorage() {
    QTemporaryDir directory;
    assert(directory.isValid());
    const auto path = directory.filePath("favorites.json");
    {
        FileFavoriteStore store(path);
        assert(store.load().empty());
        store.save({"ode-one", "twinkle"});
    }
    FileFavoriteStore restored(path);
    assert((restored.load() == std::set<std::string>{"ode-one", "twinkle"}));

    QFile file(path);
    assert(file.open(QIODevice::WriteOnly));
    assert(file.write("{broken") == 7);
    file.close();
    bool rejected = false;
    try {
        restored.load();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void checkFavoriteStorageRecovery() {
    QTemporaryDir directory;
    assert(directory.isValid());
    const auto parent = directory.filePath("blocked");
    const auto path = parent + "/favorites.json";
    QFile blocker(parent);
    assert(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    FileFavoriteStore store(path);
    bool rejected = false;
    try {
        store.save({"one"});
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
    assert(!QFile::exists(path));

    assert(QFile::remove(parent));
    assert(QDir().mkpath(parent));
    QCoreApplication::processEvents();
    assert(!QFile::exists(path)); // Failed saves cannot be flushed later.
    store.save({"two"});          // The same adapter must recover after an error.
    FileFavoriteStore restored(path);
    assert((restored.load() == std::set<std::string>{"two"}));

    // Invalid input must leave the committed snapshot unchanged.
    QFile saved(path);
    assert(saved.open(QIODevice::ReadOnly));
    const auto before = saved.readAll();
    saved.close();
    rejected = false;
    try {
        store.save({""});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    assert(saved.open(QIODevice::ReadOnly));
    assert(saved.readAll() == before);
}

void checkLegacyFavorites() {
    QTemporaryDir directory;
    assert(directory.isValid());
    // The temporary settings path and unique application name isolate old data.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("SelfGuitarTests");
    QCoreApplication::setApplicationName(QDir(directory.path()).dirName());
    {
        QSettings legacy;
        legacy.setValue("favorites", QStringList{"one", "two"});
        legacy.sync();
        assert(legacy.status() == QSettings::NoError);
    }
    FileFavoriteStore legacyReader;
    assert((legacyReader.load() == std::set<std::string>{"one", "two"}));
    FileFavoriteStore migrated(directory.filePath("favorites.json"));
    migrated.save(legacyReader.load());
    assert((migrated.load() == std::set<std::string>{"one", "two"}));
}

void checkCatalogStorage() {
    QTemporaryDir directory;
    assert(directory.isValid());
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
    assert(catalog.catalogError().empty());
    assert(catalog.favoritesError().empty());
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
    transport.receive("READY 1 96 SIMULATED\n");
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
    auto* play = window->findChild<QObject*>("playScore");
    auto* stop = window->findChild<QObject*>("stopPlayback");
    assert(play && stop);
    assert(play->property("enabled").toBool());
    assert(QMetaObject::invokeMethod(play, "clicked"));
    assert(transport.sent.back() == "PLAY\n");
    transport.receive("OK PLAY\n");
    QCoreApplication::processEvents();
    assert(transferView.playing() && transferView.busy());
    assert(stop->property("visible").toBool());
    assert(!play->property("enabled").toBool());
    assert(transferView.status().contains("Симуляция"));
    assert(QMetaObject::invokeMethod(stop, "clicked"));
    assert(transport.sent.back() == "STOP\n");
    transport.receive("OK STOP\n");
    QCoreApplication::processEvents();
    assert(!transferView.playing() && transferView.canPlay());
    transferView.play();
    transport.receive("OK PLAY\nEVENT DONE\n");
    assert(transferView.canPlay());
    assert(transferView.status().contains("завершена"));

    for (const auto& warning : warnings) {
        std::cerr << warning.toStdString() << '\n';
    }
    assert(warnings.empty());
}

QVariantMap currentArrangement(QObject* window) {
    auto current = window->property("current");
    if (current.metaType() == QMetaType::fromType<QJSValue>()) {
        current = current.value<QJSValue>().toVariant();
    }
    return current.toMap();
}

void checkCatalogReload() {
    test::MemoryCatalog repository;
    test::MemoryFavorites favorites;
    CatalogService catalog(repository, favorites);
    test::FakeTransport transport;
    TransferSession session(transport);
    UploadArrangement upload(catalog, session);
    CatalogViewModel view(catalog);
    TransferViewModel transferView(session, transport, upload);
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
    engine.rootContext()->setContextProperty("catalog", &view);
    engine.rootContext()->setContextProperty("transfer", &transferView);
    engine.load(QUrl::fromLocalFile(GUITAR_QML_FILE));
    assert(!engine.rootObjects().empty());
    auto* window = engine.rootObjects().front();
    auto* tempo = window->findChild<QObject*>("playbackTempo");
    assert(tempo);
    assert(currentArrangement(window)["title"] == "Test song");
    tempo->setProperty("value", 135);

    repository.data.songs[0].title = "Updated title";
    assert(catalog.reload()); // Changes need not originate from the view model.
    QCoreApplication::processEvents();
    assert(currentArrangement(window)["title"] == "Updated title");
    assert(view.selectedArrangementId() == "one");
    assert(tempo->property("value").toInt() == 135);
    assert(catalog.toggleFavorite("one"));
    QCoreApplication::processEvents();
    assert(currentArrangement(window)["favorite"].toBool());

    repository.data.arrangements.erase(repository.data.arrangements.begin());
    assert(view.reload());
    QCoreApplication::processEvents();
    assert(view.selectedArrangementId() == "two");
    assert(currentArrangement(window)["id"] == "two");
    assert(tempo->property("value").toInt() == 100);

    repository.data = {};
    assert(catalog.reload());
    QCoreApplication::processEvents();
    assert(view.selectedArrangementId().isEmpty());
    assert(view.selectedArrangement().empty());
    assert(currentArrangement(window).empty());
    assert(tempo->property("value").toInt() == 100);

    repository.data = test::sampleCatalog();
    assert(catalog.reload());
    QCoreApplication::processEvents();
    assert(view.selectedArrangementId() == "one");
    assert(currentArrangement(window)["title"] == "Test song");
    assert(tempo->property("value").toInt() == 120);

    repository.data.songs.clear();
    assert(!view.reload());
    assert(view.error().startsWith("Ошибка каталога:"));
    assert(catalog.toggleFavorite("one"));
    assert(view.error().startsWith("Ошибка каталога:"));
    QCoreApplication::processEvents();
    assert(currentArrangement(window)["title"] == "Test song");
    for (const auto& warning : warnings) {
        std::cerr << warning.toStdString() << '\n';
    }
    assert(warnings.empty());
}

void checkCatalogSubscriptions() {
    test::MemoryCatalog repository;
    test::MemoryFavorites favorites;
    favorites.failLoad = true;
    CatalogService catalog(repository, favorites);
    CatalogViewModel view(catalog);
    assert(view.arrangements().size() == 2);
    assert(view.error().startsWith("Ошибка избранного:"));
    {
        CatalogViewModel anotherView(catalog);
        favorites.failLoad = false;
        anotherView.toggleFavorite("one");
        assert(view.selectedArrangement()["favorite"].toBool());
        assert(view.error().isEmpty());
    }
    assert(catalog.toggleFavorite("one"));
    assert(!view.selectedArrangement()["favorite"].toBool());
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    checkFavoriteStorage();
    checkFavoriteStorageRecovery();
    checkCatalogStorage();
    checkMissingSerialPort();
    checkPresentation();
    checkCatalogReload();
    checkCatalogSubscriptions();
    checkLegacyFavorites();
    std::cout << "PASS: JSON validation, persisted favorites, QML filters, "
                 "storage recovery, selection, catalog reload, upload bindings\n";
}
