// player_types.cpp: Fixture-Bibliothek des Projekts und Drehung von Fixtures (Teil des Players).
//
// Warum es das gibt: Die Bauform eines Fixtures (zum Beispiel "Matrix 5x5") soll einmal beschrieben und dann beliebig
// oft auf der Bühne verteilt werden. Ändert man die Bauform, folgen alle Exemplare. Alles läuft über
// Player::editProject (ändern, Ausgabeplan prüfen, bei Fehler zurückrollen, ein Undo-Schritt).
#include <algorithm>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidArgument, std::move(message)}; }

Result<void> checkTypeName(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        return invalid("Der Name der Bauform ist leer oder länger als 64 Zeichen");
    }
    if (std::ranges::any_of(name, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; })) {
        return invalid("Der Name der Bauform enthält Steuerzeichen");
    }
    return {};
}

project::FixtureType* findType(project::Project& project, const std::string& name) {
    const auto it = std::ranges::find(project.types, name, &project::FixtureType::name);
    return it == project.types.end() ? nullptr : &*it;
}

Error unknownType(const std::string& name) {
    return Error{ErrorCode::NotFound, "Es gibt keine Bauform \"" + name + "\""};
}

}  // namespace

const std::vector<project::FixtureType>& Player::fixtureTypes() const {
    static const std::vector<project::FixtureType> kNone;
    return project_ ? project_->types : kNone;
}

Result<void> Player::addFixtureType(std::string name, mapping::FixtureShape shape) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        if (const Result<void> ok = checkTypeName(name); !ok.ok()) {
            return ok;
        }
        if (findType(p, name) != nullptr) {
            return Error{ErrorCode::AlreadyExists, "Es gibt schon eine Bauform \"" + name + "\""};
        }
        if (const Result<void> valid = mapping::validateShape(shape); !valid.ok()) {
            return valid;
        }
        p.types.push_back(project::FixtureType{std::move(name), std::move(shape)});
        return {};
    });
}

Result<void> Player::replaceFixtureType(const std::string& name, mapping::FixtureShape shape) {
    return editProject("ftype:" + name, [&](project::Project& p) -> Result<void> {
        project::FixtureType* type = findType(p, name);
        if (type == nullptr) {
            return unknownType(name);
        }
        if (const Result<void> valid = mapping::validateShape(shape); !valid.ok()) {
            return valid;
        }
        type->shape = shape;
        for (const mapping::Fixture& fixture : std::vector<mapping::Fixture>{p.layout.fixtures()}) {
            if (fixture.type == name) {
                if (const Result<void> r = p.layout.replaceShape(fixture.id, shape); !r.ok()) {
                    return r;
                }
            }
        }
        return {};
    });
}

Result<void> Player::renameFixtureType(const std::string& from, std::string to) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        project::FixtureType* type = findType(p, from);
        if (type == nullptr) {
            return unknownType(from);
        }
        if (const Result<void> ok = checkTypeName(to); !ok.ok()) {
            return ok;
        }
        if (to != from && findType(p, to) != nullptr) {
            return Error{ErrorCode::AlreadyExists, "Es gibt schon eine Bauform \"" + to + "\""};
        }
        for (const mapping::Fixture& fixture : std::vector<mapping::Fixture>{p.layout.fixtures()}) {
            if (fixture.type == from) {
                (void)p.layout.setType(fixture.id, to);
            }
        }
        type->name = std::move(to);
        return {};
    });
}

Result<void> Player::removeFixtureType(const std::string& name) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        if (findType(p, name) == nullptr) {
            return unknownType(name);
        }
        // Die Exemplare behalten ihre Form und werden zu Fixtures mit eigener Form.
        for (const mapping::Fixture& fixture : std::vector<mapping::Fixture>{p.layout.fixtures()}) {
            if (fixture.type == name) {
                (void)p.layout.setType(fixture.id, std::string{});
            }
        }
        std::erase_if(p.types, [&name](const project::FixtureType& type) { return type.name == name; });
        return {};
    });
}

Result<void> Player::assignFixtureType(mapping::FixtureId id, const std::string& typeName) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        if (p.layout.find(id) == nullptr) {
            return Error{ErrorCode::NotFound, "Fixture unbekannt"};
        }
        if (typeName.empty()) {
            return p.layout.setType(id, std::string{});  // löst das Fixture von der Bibliothek, die Form bleibt
        }
        const project::FixtureType* type = findType(p, typeName);
        if (type == nullptr) {
            return unknownType(typeName);
        }
        if (const Result<void> r = p.layout.replaceShape(id, type->shape); !r.ok()) {
            return r;
        }
        return p.layout.setType(id, typeName);
    });
}

Result<mapping::FixtureId> Player::addFixtureOfType(std::string name, const std::string& typeName, const Vec3& origin,
                                                    const Vec3& rotation, std::optional<output::FixturePatch> patch) {
    mapping::FixtureId created;
    const Result<void> r = editProject({}, [&](project::Project& p) -> Result<void> {
        const project::FixtureType* type = findType(p, typeName);
        if (type == nullptr) {
            return unknownType(typeName);
        }
        if (name.empty() || std::ranges::any_of(p.layout.fixtures(), [&name](const mapping::Fixture& f) { return f.name == name; })) {
            return invalid("Der Name des Fixtures ist leer oder schon vergeben");
        }
        const auto id = p.layout.add(name, origin, type->shape);
        if (!id.ok()) {
            return id.error();
        }
        created = id.value();
        if (const Result<void> rotated = p.layout.rotate(created, rotation); !rotated.ok()) {
            return rotated;
        }
        (void)p.layout.setType(created, typeName);
        if (patch) {
            patch->fixture = created;
            p.patches.push_back(*patch);
        }
        return {};
    });
    if (!r.ok()) {
        return r.error();
    }
    return created;
}

Result<void> Player::setFixtureRotation(mapping::FixtureId id, const Vec3& degrees) {
    return editProject("frotate:" + std::to_string(id.value()),
                       [&](project::Project& p) { return p.layout.rotate(id, degrees); });
}

}  // namespace whitehole::runtime
