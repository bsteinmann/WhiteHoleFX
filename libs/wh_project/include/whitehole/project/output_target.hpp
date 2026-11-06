// output_target.hpp: Aus dem Ziel eines Output-Nodes (Fixtures, Gruppen, Regionen) wird sein Rechteck.
//
// Warum es das gibt: Ein Output-Node bespielt ein Rechteck der Leinwand (left/top/width/height). Wer ihm
// stattdessen Fixtures, Gruppen oder Regionen zuweist, will nicht rechnen: Das Rechteck ist das kleinste, das
// alle gewählten LEDs (samt halber Zelle Rand) und Regionen umschließt. Es wird in die Parameter des Nodes
// geschrieben, so bleibt die Datei ohne Zusatzwissen lesbar und der Compiler braucht nichts Neues. Ändert sich
// das Layout, wird es neu berechnet (applyOutputTargets).
#pragma once

#include <string>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/mapping/led_map.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project {

/// Rechteck in Anteilen der Leinwand (0..1), `top` zählt von oben wie beim Output-Node.
struct TargetRect {
    float left{0.0F};
    float top{0.0F};
    float width{1.0F};
    float height{1.0F};
};

struct TargetResolution {
    TargetRect rect;
    std::vector<std::string> unknown;  // Einträge, die kein Fixture/keine Gruppe/Region (mehr) treffen
};

/// Das Rechteck für eine Zielliste. Fehler: InvalidArgument, wenn kein einziger Eintrag etwas trifft.
[[nodiscard]] Result<TargetResolution> resolveTargets(const Project& project, const mapping::LedMap& map,
                                                      const std::vector<std::string>& targets);

/// Setzt left/top/width/height aller Output-Nodes mit Ziel (Hauptgraph und Looks). Nodes, deren Ziele nichts mehr
/// treffen, behalten ihr Rechteck. Liefert die Namen der Einträge ohne Treffer ("Node n3: fixture:Alt").
std::vector<std::string> applyOutputTargets(Project& project, const mapping::LedMap& map);

/// Benennt ein Fixture in allen Gruppen und Output-Zielen (Hauptgraph und Looks) um, damit die Verweise gültig bleiben.
void renameFixtureRefs(Project& project, const std::string& from, const std::string& to);

}  // namespace whitehole::project
