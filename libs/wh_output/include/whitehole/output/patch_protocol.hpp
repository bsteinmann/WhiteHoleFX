// patch_protocol.hpp: einen Patch auf das Protokoll seines Controllers umstellen.
//
// Warum es das gibt: Das Protokoll gehört zum Controller (zur Zieladresse), nicht zum einzelnen Fixture. Wechselt ein
// Controller von Art-Net auf sACN oder zieht ein Fixture auf einen Controller mit anderem Protokoll, müssen die Patches
// nachziehen. Art-Net zählt Universen ab 0, sACN ab 1; Pulte und Controller zeigen für dasselbe Universe aber meist
// dieselbe Nummer. Darum verschiebt die Umstellung zwischen Art-Net und sACN die Nummer um 1, damit das "gleiche"
// Universe gemeint bleibt (Art-Net 0 = sACN 1).
#pragma once

#include "whitehole/core/result.hpp"
#include "whitehole/output/output_plan.hpp"

namespace whitehole::output {

/// Kleinste Universe-Nummer des Protokolls: Art-Net 0, sACN 1 (DDP kennt keine Universen: 0).
[[nodiscard]] constexpr std::uint16_t firstUniverse(Protocol protocol) noexcept {
    return protocol == Protocol::Sacn ? 1 : 0;
}

/// Größte Universe-Nummer: Art-Net 32767, sACN 63999 (DDP: 0).
[[nodiscard]] constexpr std::uint16_t lastUniverse(Protocol protocol) noexcept {
    switch (protocol) {
        case Protocol::ArtNet: return 32767;
        case Protocol::Sacn: return 63999;
        case Protocol::Ddp: return 0;
    }
    return 0;
}

/// Stellt `patch` auf `protocol` um. Art-Net -> sACN: Universe + 1; sACN -> Art-Net: Universe - 1; von und nach DDP bleibt
/// die Nummer, soweit sie im Bereich des Ziels liegt (sonst die kleinste gültige). Liefert die Verschiebung (-1, 0, +1).
/// Fehler: InvalidArgument, wenn das Universe im Zielprotokoll nicht existiert (sACN 40000 gibt es bei Art-Net nicht).
[[nodiscard]] Result<int> alignPatchProtocol(FixturePatch& patch, Protocol protocol);

}  // namespace whitehole::output
