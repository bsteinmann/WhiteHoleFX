// project_stage.hpp (privat): Zeilen für Gruppen und Regionen sowie Ziele am Output-Node.
//
// Warum es das gibt: Mehrere Outputs brauchen Namen, auf die sie zeigen können. Die Grammatik ist klein und
// steht mit dem Schreiber zusammen, damit beide Richtungen zusammenpassen.
//   group "Name" "Fixture 1" "Fixture 2" ...      (mindestens ein Fixture)
//   region "Name" <links> <oben> <breite> <höhe>   (Anteile 0..1, oben zählt von oben)
//   target fixture|group|region "Name"             (im node-Block eines Output-Nodes)
#pragma once

#include <ostream>
#include <string>

#include "project_text.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project::detail {

FixtureGroup parseGroup(Reader& r);
StageRegion parseRegion(Reader& r);
/// Liest "fixture|group|region "Name"" und liefert den Eintrag "art:Name" für NodeInstance::targets.
std::string parseTarget(Reader& r);

void writeGroup(std::ostream& out, const FixtureGroup& group);
void writeRegion(std::ostream& out, const StageRegion& region);
/// Schreibt die "target"-Zeilen eines Nodes (je Eintrag eine Zeile).
void writeTargets(std::ostream& out, const std::vector<std::string>& targets);

}  // namespace whitehole::project::detail
