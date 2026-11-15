// dmx_output_usage.hpp: wer einen gesendeten DMX-Kanal belegt (Fixture-Patch oder DMX-Kanal-Pool).
//
// Warum es das gibt: Schritt 3 des DMX-Patch/Monitor-Gitters (siehe CLAUDE.md) - Rechtsklick auf eine
// Zelle im DMX-Ausgangs-Monitor soll zeigen, was dort schon gepatcht ist, bevor man versehentlich einen
// zweiten Patch auf dieselben Kanäle legt. Reine Lesefunktion, ändert nichts am Projekt; dieselbe
// vereinfachte Annahme wie output::SentDmxStore: das Ziel (destination) wird ignoriert, nur Protokoll
// und Universe zählen - der Monitor selbst unterscheidet ja auch keine Ziele.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/output/output_plan.hpp"

namespace whitehole::runtime {

struct DmxOutputOccupant {
    enum class Kind { Fixture, Pool };
    Kind kind{Kind::Fixture};
    std::string name;              // Fixture-Name (Kind::Fixture); bei Kind::Pool leer
    std::uint16_t poolChannel{0};  // nur bei Kind::Pool: erster Pool-Kanal des gefundenen Patches
};

/// Sucht, was den angegebenen Kanal (Protokoll + Universe + 1-basierter Kanal) belegt: ein Fixture-Patch
/// (über alle Universen, die sein Strip/Grid ggf. überspannt, wie OutputPlan::create es auch täte) oder ein
/// Eintrag des DMX-Kanal-Pools. Nullopt heißt nur: dieses Projekt patcht nichts dorthin - der Kanal könnte
/// trotzdem belegt sein, wenn ihn ein anderes Programm oder Gerät am selben Netz sendet, das kann diese
/// Funktion nicht wissen.
[[nodiscard]] std::optional<DmxOutputOccupant> findDmxOutputOccupant(
    const mapping::Layout& layout, std::span<const output::FixturePatch> fixturePatches,
    std::span<const output::DmxChannelPatch> dmxPatches, output::Protocol protocol, std::uint32_t universe,
    std::uint32_t channel);

}  // namespace whitehole::runtime
