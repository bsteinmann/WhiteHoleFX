// fixture_shape_label.hpp: kurze Bezeichnung der Grundform eines Fixtures (Matrix, Ring, Strip, ...).
//
// Warum es das gibt: In den Bauform-Listen (Fixture-Bibliothek, Fixtures in Show) steht als Beschriftung der frei
// vergebene Name der Bauform (z. B. "Kachel 5x3"); der verrät aber nicht, welche Grundform dahintersteckt, sobald
// der Name nicht mehr danach klingt. Diese kleine Hilfsfunktion liefert die Original-Bezeichnung der Form (dieselben
// Begriffe wie im Formular der Bauform, siehe shape_form.cpp), damit sie neben dem Namen stehen bleibt und man auch
// bei frei gewählten Namen noch sieht, was es ist.
#pragma once

#include <QString>

#include "whitehole/mapping/fixture.hpp"

namespace whitehole::studio {

/// "2D-Matrix", "3D-Matrix", "Strip", "Kreis", "Spirale", "Säule" oder "Punktwolke".
[[nodiscard]] QString shapeKindLabel(const mapping::FixtureShape& shape);

}  // namespace whitehole::studio
