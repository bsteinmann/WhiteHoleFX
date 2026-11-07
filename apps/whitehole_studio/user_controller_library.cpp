// user_controller_library.cpp: Benutzerbibliothek der Controller auf der Festplatte (Teil von user_library.hpp).
//
// Warum eine eigene Datei: user_library.cpp bleibt bei den Bauformen; hier steht dieselbe Logik (atomares Schreiben,
// defekte Datei bleibt unangetastet) für Controller, damit man verschiedene Aufbauten (Wand-ESP, Säule, Studio-Controller)
// einmal anlegt und in jedem Projekt wiederfindet.
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

#include "user_library.hpp"

namespace whitehole::studio::userlib {

namespace {

Error ioError(const QString& what) { return Error{ErrorCode::IoError, what.toStdString()}; }

Result<void> saveControllers(const std::vector<output::Destination>& controllers) {
    const QString file = controllerPath();
    if (!QDir().mkpath(QFileInfo{file}.absolutePath())) {
        return ioError(QObject::tr("Ordner der Benutzerbibliothek lässt sich nicht anlegen: %1").arg(file));
    }
    QSaveFile out{file};
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return ioError(QObject::tr("Benutzerbibliothek lässt sich nicht schreiben: %1").arg(file));
    }
    out.write(QByteArray::fromStdString(project::writeControllerLibrary(controllers)));
    if (!out.commit()) {
        return ioError(QObject::tr("Benutzerbibliothek lässt sich nicht speichern: %1").arg(file));
    }
    return {};
}

}  // namespace

QString controllerPath() {
    const QString overridePath = qEnvironmentVariable("WHITEHOLEFX_CONTROLLER_LIBRARY");
    if (!overridePath.isEmpty()) {
        return overridePath;
    }
    return QDir{QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)}.filePath(
        QStringLiteral("controller-library.whfx"));
}

Result<std::vector<output::Destination>> loadControllers(std::shared_ptr<const graph::INodeRegistry> registry) {
    QFile file{controllerPath()};
    if (!file.exists()) {
        return std::vector<output::Destination>{};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return ioError(QObject::tr("Benutzerbibliothek lässt sich nicht lesen: %1").arg(controllerPath()));
    }
    const QByteArray bytes = file.readAll();
    return project::parseControllerLibrary(std::string_view{bytes.constData(), static_cast<std::size_t>(bytes.size())},
                                           std::move(registry));
}

Result<void> upsertController(std::shared_ptr<const graph::INodeRegistry> registry, const output::Destination& controller) {
    if (controller.name.empty()) {
        return Error{ErrorCode::InvalidArgument, QObject::tr("Der Controller braucht einen Namen, um gespeichert zu werden").toStdString()};
    }
    auto controllers = loadControllers(std::move(registry));
    if (!controllers.ok()) {
        return controllers.error();
    }
    const auto found = std::ranges::find(controllers.value(), controller.name, &output::Destination::name);
    if (found != controllers.value().end()) {
        *found = controller;
    } else {
        controllers.value().push_back(controller);
    }
    return saveControllers(controllers.value());
}

Result<void> removeController(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& name) {
    auto controllers = loadControllers(std::move(registry));
    if (!controllers.ok()) {
        return controllers.error();
    }
    const auto before = controllers.value().size();
    std::erase_if(controllers.value(), [&name](const output::Destination& d) { return d.name == name; });
    return controllers.value().size() == before ? Result<void>{} : saveControllers(controllers.value());
}

}  // namespace whitehole::studio::userlib
