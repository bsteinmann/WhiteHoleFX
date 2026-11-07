// player_layout.cpp: Layout, Ausgabe und Einstellungen bearbeiten (Teil des Players).
//
// Alles läuft über Player::editProject: ändern, den Ausgabeplan prüfen, bei einem Fehler
// zurückrollen. So kann der Layout-Editor dem Nutzer nie ein Projekt hinterlassen, das sich nicht
// mehr starten lässt.
#include <algorithm>
#include <cmath>

#include "whitehole/project/output_target.hpp"
#include "whitehole/output/patch_protocol.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidArgument, std::move(message)}; }

Result<void> checkName(const project::Project& project, const std::string& name, mapping::FixtureId self) {
    if (name.empty()) {
        return invalid("Der Name des Fixtures darf nicht leer sein");
    }
    for (const mapping::Fixture& fixture : project.layout.fixtures()) {
        if (fixture.name == name && fixture.id != self) {
            return invalid("Ein Fixture heißt schon \"" + name + "\"");
        }
    }
    return {};
}

}  // namespace

Result<mapping::FixtureId> Player::addFixture(std::string name, const Vec3& origin, mapping::FixtureShape shape,
                                              std::optional<output::FixturePatch> patch) {
    mapping::FixtureId created;
    const Result<void> r = editProject({}, [&](project::Project& p) -> Result<void> {
        if (const Result<void> ok = checkName(p, name, mapping::FixtureId{}); !ok.ok()) {
            return ok;
        }
        const auto id = p.layout.add(name, origin, std::move(shape));
        if (!id.ok()) {
            return id.error();
        }
        created = id.value();
        if (patch) {
            patch->fixture = created;
            p.patches.push_back(*patch);
            if (const Result<int> aligned = project::alignPatches(p); !aligned.ok()) {
                return aligned.error();
            }
        }
        return {};
    });
    if (!r.ok()) {
        return r.error();
    }
    return created;
}

Result<void> Player::removeFixture(mapping::FixtureId id) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        if (const Result<void> r = p.layout.remove(id); !r.ok()) {
            return r;
        }
        std::erase_if(p.patches, [id](const output::FixturePatch& patch) { return patch.fixture == id; });
        return {};
    });
}

Result<void> Player::renameFixture(mapping::FixtureId id, std::string name) {
    return editProject("fname:" + std::to_string(id.value()), [&](project::Project& p) -> Result<void> {
        if (const Result<void> ok = checkName(p, name, id); !ok.ok()) {
            return ok;
        }
        std::string oldName;
        for (const mapping::Fixture& fixture : p.layout.fixtures()) {
            if (fixture.id == id) {
                oldName = fixture.name;
            }
        }
        const std::string newName = name;
        if (const Result<void> r = p.layout.rename(id, std::move(name)); !r.ok()) {
            return r;
        }
        project::renameFixtureRefs(p, oldName, newName);
        return Result<void>{};
    });
}

Result<void> Player::moveFixture(mapping::FixtureId id, const Vec3& origin) {
    return editProject("fmove:" + std::to_string(id.value()),
                       [&](project::Project& p) { return p.layout.move(id, origin); });
}

Result<void> Player::setFixtureShape(mapping::FixtureId id, mapping::FixtureShape shape) {
    return editProject("fshape:" + std::to_string(id.value()),
                       [&](project::Project& p) { return p.layout.replaceShape(id, std::move(shape)); });
}

Result<void> Player::setPatch(mapping::FixtureId id, std::optional<output::FixturePatch> patch) {
    return editProject("fpatch:" + std::to_string(id.value()), [&](project::Project& p) -> Result<void> {
        if (p.layout.find(id) == nullptr) {
            return Error{ErrorCode::NotFound, "Fixture unbekannt"};
        }
        std::erase_if(p.patches, [id](const output::FixturePatch& existing) { return existing.fixture == id; });
        if (patch) {
            patch->fixture = id;
            p.patches.push_back(*patch);
            // Das Protokoll gehört zum Controller: zieht das Fixture auf einen anderen, folgt der Patch ihm.
            if (const Result<int> aligned = project::alignPatches(p); !aligned.ok()) {
                return aligned.error();
            }
        }
        return {};
    });
}

Result<void> Player::setDestinations(std::vector<output::Destination> destinations) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        for (const output::Destination& d : destinations) {
            if (!d.address.empty() && !output::isIpv4(d.address)) {
                return invalid("\"" + d.address + "\" ist keine IPv4-Adresse");
            }
            if (d.address.empty() && d.protocol != output::Protocol::Sacn) {
                return invalid("Ohne Adresse (Multicast) geht nur sACN; für Art-Net und DDP eine IPv4-Adresse eintragen");
            }
        }
        p.destinations = std::move(destinations);
        // Ein Controller mit neuem Protokoll nimmt seine Patches mit (Art-Net <-> sACN: Universe +-1).
        if (const Result<int> aligned = project::alignPatches(p); !aligned.ok()) {
            return aligned.error();
        }
        return {};
    });
}

output::FixturePatch Player::suggestPatch(std::uint64_t leds) const {
    (void)leds;  // die Größe des neuen Fixtures bestimmt nur, was hinter ihm frei ist
    output::FixturePatch patch;
    // Der Vorschlag folgt dem Protokoll von Controller 0 (der neue Patch landet dort).
    if (project_ && !project_->destinations.empty()) {
        patch.protocol = project_->destinations[0].protocol;
    }
    std::uint64_t next = output::firstUniverse(patch.protocol);
    if (project_) {
        for (const output::FixturePatch& existing : project_->patches) {
            if (existing.protocol == output::Protocol::Ddp || existing.destination != 0) {
                continue;
            }
            const mapping::Fixture* fixture = project_->layout.find(existing.fixture);
            const std::uint64_t count = fixture != nullptr ? mapping::ledCount(fixture->shape) : 0;
            next = std::max<std::uint64_t>(next, existing.universe + (count + 169) / 170);
        }
    }
    patch.universe = static_cast<std::uint16_t>(
        std::min<std::uint64_t>(next, std::max<std::uint16_t>(output::lastUniverse(patch.protocol), output::firstUniverse(patch.protocol))));
    return patch;
}

Result<void> Player::setSettings(const Settings& settings) {
    if (!(settings.framesPerSecond > 0.0 && settings.framesPerSecond <= 1000.0) ||
        !std::isfinite(settings.framesPerSecond)) {
        return invalid("Die Bildrate muss größer 0 und höchstens 1000 sein");
    }
    if (settings.sacnPriority > 200) {
        return invalid("Die sACN-Priorität liegt zwischen 0 und 200");
    }
    if (settings.sourceName.empty()) {
        return invalid("Der Quellname darf nicht leer sein");
    }
    return editProject("settings", [&](project::Project& p) -> Result<void> {
        p.framesPerSecond = settings.framesPerSecond;
        p.normalize = settings.normalize;
        p.sacnPriority = settings.sacnPriority;
        p.sourceName = settings.sourceName;
        return {};
    });
}

}  // namespace whitehole::runtime
