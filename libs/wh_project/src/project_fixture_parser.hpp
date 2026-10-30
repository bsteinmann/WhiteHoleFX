// project_fixture_parser.hpp (privat): Zeilen innerhalb eines `fixture`-Blocks.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "project_text.hpp"
#include "whitehole/mapping/fixture.hpp"
#include "whitehole/output/output_plan.hpp"

namespace whitehole::project {
struct Project;
}

namespace whitehole::project::detail {

/// Das, was von einem Fixture-Block bis `end` gesammelt wurde.
struct FixtureDraft {
    std::size_t line{0};
    std::string name;
    Vec3 origin;
    std::optional<mapping::FixtureShape> shape;
    std::vector<Vec3> points;
    std::vector<output::FixturePatch> patches;
};

/// `fixture "Name" [at x y z]` (Reader steht hinter dem Schlüsselwort).
FixtureDraft beginFixture(Reader& r);

/// Eine Anweisung im Block (matrix, strip, grid, point, patch). Unbekannt: Fehler im Reader.
void fixtureStatement(const std::string& keyword, Reader& r, FixtureDraft& draft);

/// `end`: legt das Fixture im Layout an und übernimmt die Patches.
std::optional<Error> finishFixture(FixtureDraft& draft, Project& project);

}  // namespace whitehole::project::detail
