// user_library.cpp: Umsetzung zu user_library.hpp.
#include "user_library.hpp"

#include <QDir>
#include <QFile>
#include <QObject>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace whitehole::studio::userlib {

namespace {

Error ioError(const QString& what) { return Error{ErrorCode::IoError, what.toStdString()}; }

Result<void> save(const std::vector<project::FixtureType>& types) {
    const QString file = path();
    if (!QDir().mkpath(QFileInfo{file}.absolutePath())) {
        return ioError(QObject::tr("Ordner der Benutzerbibliothek lässt sich nicht anlegen: %1").arg(file));
    }
    QSaveFile out{file};
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return ioError(QObject::tr("Benutzerbibliothek lässt sich nicht schreiben: %1").arg(file));
    }
    out.write(QByteArray::fromStdString(project::writeFixtureLibrary(types)));
    if (!out.commit()) {
        return ioError(QObject::tr("Benutzerbibliothek lässt sich nicht speichern: %1").arg(file));
    }
    return {};
}

}  // namespace

QString path() {
    const QString overridePath = qEnvironmentVariable("WHITEHOLEFX_FIXTURE_LIBRARY");
    if (!overridePath.isEmpty()) {
        return overridePath;
    }
    return QDir{QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)}.filePath(
        QStringLiteral("fixture-library.whfx"));
}

Result<std::vector<project::FixtureType>> load(std::shared_ptr<const graph::INodeRegistry> registry) {
    QFile file{path()};
    if (!file.exists()) {
        return std::vector<project::FixtureType>{};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return ioError(QObject::tr("Benutzerbibliothek lässt sich nicht lesen: %1").arg(path()));
    }
    const QByteArray bytes = file.readAll();
    return project::parseFixtureLibrary(std::string_view{bytes.constData(), static_cast<std::size_t>(bytes.size())},
                                        std::move(registry));
}

Result<void> upsert(std::shared_ptr<const graph::INodeRegistry> registry, const project::FixtureType& type) {
    auto types = load(std::move(registry));
    if (!types.ok()) {
        return types.error();
    }
    const auto found = std::ranges::find(types.value(), type.name, &project::FixtureType::name);
    if (found != types.value().end()) {
        found->shape = type.shape;
    } else {
        types.value().push_back(type);
    }
    return save(types.value());
}

Result<void> remove(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& name) {
    auto types = load(std::move(registry));
    if (!types.ok()) {
        return types.error();
    }
    const auto before = types.value().size();
    std::erase_if(types.value(), [&name](const project::FixtureType& t) { return t.name == name; });
    return types.value().size() == before ? Result<void>{} : save(types.value());
}

Result<void> rename(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& from, const std::string& to) {
    auto types = load(std::move(registry));
    if (!types.ok()) {
        return types.error();
    }
    const auto found = std::ranges::find(types.value(), from, &project::FixtureType::name);
    if (found == types.value().end()) {
        return {};
    }
    project::FixtureType moved = *found;
    moved.name = to;
    std::erase_if(types.value(), [&](const project::FixtureType& t) { return t.name == from || t.name == to; });
    types.value().push_back(std::move(moved));
    return save(types.value());
}

}  // namespace whitehole::studio::userlib
