// patch_space.hpp: Welche Kanäle belegt ein gepatchtes Fixture, und wo ist noch Platz?
//
// Warum es diese Datei gibt: Beim Patchen will man sehen, welchen Adressraum ein Fixture einnimmt ("Universe 1,
// Kanal 1 bis 297"), und ein zweites Fixture in dasselbe Universe hinter das erste legen (eine Matrix und danach ein
// einzelner Pixel am selben Controller). Beides braucht dieselbe Rechnung, die auch der OutputPlan macht
// (eine LED belegt 3 Kanäle, wird nie über eine Universe-Grenze geteilt, ein Fixture läuft ab Kanal 1 im nächsten
// Universe weiter). Sie steht hier einmal, damit Anzeige, Vorschlag und Ausgabe nicht auseinanderlaufen.
//
// Aufgabe:
//   - `addressSpan`: erster und letzter belegter Kanal eines Patches (für die Anzeige).
//   - `channelBlocks`: die belegten Kanalbereiche je Universe (für die Prüfung auf Überlappung).
//   - `fitChannel`: wo ein Patch hinpasst, wenn sein Wunschkanal belegt ist.
//
// Abhängigkeiten: output_plan.hpp (FixturePatch).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "whitehole/output/output_plan.hpp"

namespace whitehole::output {

/// Belegte Kanäle in einem Universe: Bytes [firstByte, endByte) ab 0 (Kanal = Byte + 1).
struct ChannelBlock {
    Protocol protocol{Protocol::ArtNet};
    std::uint32_t destination{0};
    std::uint32_t universe{0};
    std::uint32_t firstByte{0};
    std::uint32_t endByte{0};
};

/// Anfang und Ende des belegten Adressraums. DMX: Universe und Kanal 1..510; DDP: Byte-Offsets (Universe 0).
struct AddressSpan {
    Protocol protocol{Protocol::ArtNet};
    std::uint32_t firstUniverse{0};
    std::uint32_t firstChannel{1};
    std::uint32_t lastUniverse{0};
    std::uint32_t lastChannel{1};
};

/// Die Kanalbereiche eines Fixtures mit `ledCount` LEDs, je Universe einer. Leer bei DDP und bei 0 LEDs.
[[nodiscard]] std::vector<ChannelBlock> channelBlocks(const FixturePatch& patch, std::uint32_t ledCount);

/// Erster und letzter belegter Kanal. Leer bei 0 LEDs.
[[nodiscard]] std::optional<AddressSpan> addressSpan(const FixturePatch& patch, std::uint32_t ledCount);

/// Der Kanal, auf dem `wanted` (Protokoll, Ziel, Universe gesetzt) frei liegt: der Wunschkanal, wenn nichts anderes
/// dort liegt, sonst direkt hinter dem letzten belegten Kanal des Universe. Leer, wenn das Fixture dort nirgends passt.
[[nodiscard]] std::optional<std::uint16_t> fitChannel(const FixturePatch& wanted, std::uint32_t ledCount,
                                                      std::span<const ChannelBlock> others);

}  // namespace whitehole::output
