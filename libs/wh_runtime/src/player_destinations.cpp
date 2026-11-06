// player_destinations.cpp: ein beliebiges Ziel (Controller) entfernen, ohne die Patches der anderen zu verschieben.
//
// Warum es das gibt: Die Patches der Fixtures verweisen mit einer Nummer auf ein Ziel. Wer ein Ziel mitten aus der Liste
// streicht, darf nicht die Nummern aller folgenden Ziele durcheinanderbringen: Fixtures auf höheren Zielen müssen
// mitrücken, und ein Ziel, auf das noch ein Fixture sendet, bleibt stehen (sonst ginge das Fixture still ins Leere).
#include <string>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

Result<void> Player::removeDestination(std::size_t index) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        if (index >= p.destinations.size()) {
            return Error{ErrorCode::NotFound, "Es gibt kein Ziel Nr. " + std::to_string(index)};
        }
        for (const output::FixturePatch& patch : p.patches) {
            if (patch.destination == index) {
                const mapping::Fixture* fixture = p.layout.find(patch.fixture);
                return Error{ErrorCode::InvalidArgument,
                             "Ziel " + std::to_string(index) + " wird noch von \"" +
                                 (fixture != nullptr ? fixture->name : std::string{"?"}) +
                                 "\" benutzt. Erst das Fixture umpatchen."};
            }
        }
        for (output::FixturePatch& patch : p.patches) {
            if (patch.destination > index) {
                --patch.destination;
            }
        }
        p.destinations.erase(p.destinations.begin() + static_cast<std::ptrdiff_t>(index));
        return {};
    });
}

}  // namespace whitehole::runtime
