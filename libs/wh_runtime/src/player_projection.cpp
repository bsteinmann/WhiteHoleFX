// player_projection.cpp: Projektion (Volume/Faces/Box) für einzelne Fixtures und ganze Gruppen setzen.
//
// Warum es diese Datei gibt: Der Layout-Editor und die Gruppenliste sollen mit einem Befehl eine ganze
// Auswahl umstellen können, und das muss genau ein Undo-Schritt sein. Die Auflösung „Gruppe → Fixtures“
// steht hier, damit sie ohne Oberfläche testbar ist.
#include <algorithm>
#include <string>
#include <vector>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

Result<void> Player::setFixturesProjection(const std::vector<mapping::FixtureId>& ids, mapping::Projection projection) {
    if (ids.empty()) {
        return Error{ErrorCode::InvalidArgument, "Keine Fixtures gewählt"};
    }
    // Ohne Schlüssel: jede Auswahl ist ein eigener Schritt, sonst würden zwei Umstellungen hintereinander verschmelzen.
    return editProject({}, [&](project::Project& p) -> Result<void> {
        for (const mapping::FixtureId id : ids) {
            if (const Result<void> r = p.layout.setProjection(id, projection); !r.ok()) {
                return r;
            }
        }
        return {};
    });
}

Result<void> Player::setGroupProjection(const std::string& group, mapping::Projection projection) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Kein Projekt geladen"};
    }
    const project::FixtureGroup* found = nullptr;
    for (const project::FixtureGroup& candidate : project_->groups) {
        if (candidate.name == group) {
            found = &candidate;
            break;
        }
    }
    if (found == nullptr) {
        return Error{ErrorCode::NotFound, "Gruppe unbekannt"};
    }
    std::vector<mapping::FixtureId> ids;
    for (const std::string& fixtureName : found->fixtures) {
        const auto& fixtures = project_->layout.fixtures();
        const auto it = std::find_if(fixtures.begin(), fixtures.end(),
                                     [&](const mapping::Fixture& fixture) { return fixture.name == fixtureName; });
        if (it == fixtures.end()) {
            return Error{ErrorCode::NotFound, "Fixture in der Gruppe unbekannt"};
        }
        ids.push_back(it->id);
    }
    return setFixturesProjection(ids, projection);
}

}  // namespace whitehole::runtime
