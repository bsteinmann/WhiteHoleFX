// led_address.hpp: Welche Adresse hat eine bestimmte LED im Netzwerk?
//
// Warum es das gibt: Wer einen Controller einrichtet, muss wissen, bei welchem Universe und Kanal eine LED
// ankommt ("LED 37 liegt in Universe 2, Kanal 109–111"). Die Rechnung steht hier einmal, nach denselben
// Regeln wie der OutputPlan (eine LED belegt 3 Kanäle und wird nie über eine Universe-Grenze geteilt),
// damit die Anzeige im Studio und die Ausgabe nicht auseinanderlaufen.
#pragma once

#include <cstdint>
#include <optional>

#include "whitehole/output/output_plan.hpp"

namespace whitehole::output {

struct LedAddress {
    Protocol protocol{Protocol::ArtNet};
    std::uint32_t destination{0};
    std::uint32_t universe{0};      // DMX-Protokolle
    std::uint32_t firstChannel{1};  // DMX: 1..510 (Rot/erster Farbkanal); DDP: Byte-Offset im Strom
    std::uint32_t lastChannel{3};   // DMX: letzter Kanal der LED; DDP: letztes Byte
};

/// Adresse der `ledInFixture`-ten LED (0-basiert, in Kabelreihenfolge) eines gepatchten Fixtures.
/// Leer, wenn der Patch das Universe-Ende überschreiten würde.
[[nodiscard]] std::optional<LedAddress> ledAddress(const FixturePatch& patch, std::uint32_t ledInFixture);

}  // namespace whitehole::output
