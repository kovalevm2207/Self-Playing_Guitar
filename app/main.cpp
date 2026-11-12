#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include "backend.h"
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("SelfGuitar"); app.setApplicationName("SelfGuitar");
    QQuickStyle::setStyle("Basic");
    Backend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("guitar", &backend);
    engine.loadFromModule("Guitar", "Main");
    if (engine.rootObjects().isEmpty()) return 1;
    const auto arguments = app.arguments();
    const int shot = arguments.indexOf("--screenshot");
    if (shot >= 0 && shot + 1 < arguments.size()) {
        const auto path = arguments.at(shot + 1);
        QTimer::singleShot(1200, &app, [&app, &engine, path] {
            auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            app.exit(window && window->grabWindow().save(path) ? 0 : 2);
        });
    }
    return app.exec();
}
