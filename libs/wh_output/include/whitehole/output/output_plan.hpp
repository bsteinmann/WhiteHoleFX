// output_plan.hpp: verteilt die LEDs eines Layouts auf Universen und Pixelströme.
//
// Warum es diese Datei gibt: Zwischen "LED Nummer i hat diese Farbe" und "Paket an
// Controller X" liegt die Zuordnung: Welches Fixture hängt an welchem Universe und ab
// welchem Kanal? Wo muss ein langer Strip auf das nächste Universe umbrechen? Diese
// Zuordnung ändert sich nur, wenn der Nutzer patcht, nicht pro Frame. Deshalb wird sie
// einmal berechnet und geprüft (OutputPlan::create); pro Frame füllt render() nur noch
// die Bytes. So fallen Konfigurationsfehler beim Patchen auf, nicht mitten im Betrieb.
//
// Aufgabe:
//   - FixturePatch beschreibt, wohin ein Fixture geht: Protokoll, Ziel, Universe, Kanal.
//   - create() prüft die Patches, schneidet jedes Fixture in Läufe (Runs) und sammelt sie
//     in Slots. Ein Slot ist ein Datenblock, der später genau ein Paket (DMX-Protokolle)
//     oder einen Paketstrom (DDP) wird.
//   - render() wandelt die Farben mit dem ColorEncoder des Patches und schreibt sie in
//     die Slot-Puffer.
//
// Regeln:
//   - Eine LED belegt 3 aufeinanderfolgende Kanäle und wird nie auf zwei Universen
//     geteilt. Ein Universe fasst daher höchstens 170 LEDs (510 Kanäle). Läuft ein
//     Fixture über, geht es ab Kanal 1 im nächsten Universe weiter.
//   - Mehrere Fixtures dürfen sich ein Universe teilen, solange sich ihre Kanäle nicht
//     überlappen.
//   - DDP kennt keine Universen: Jedes Fixture ist ein eigener Strom ab `ddpOffset`.
//     Überlappende DDP-Ströme desselben Ziels werden nicht geprüft.
//   - Fixtures ohne Patch werden nicht ausgegeben.
//
// Das Ziel (`destination`) ist nur eine Nummer; welche Adresse dahintersteckt, weiß der
// Transport. Der Plan bleibt dadurch frei von Netzwerkdingen.
//
// Abhängigkeiten: wh_core, wh_mapping (LedMap), color_encoder.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/mapping/led_map.hpp"
#include "whitehole/output/color_encoder.hpp"

namespace whitehole::output {

enum class Protocol : std::uint8_t { ArtNet, Sacn, Ddp };

struct FixturePatch {
    mapping::FixtureId fixture;
    Protocol protocol{Protocol::ArtNet};
    std::uint32_t destination{0};  // Nummer im Zielverzeichnis des Transports
    std::uint16_t universe{0};     // Art-Net: 0..32767, sACN: 1..63999; bei DDP unbenutzt
    std::uint16_t channel{1};      // erster Kanal 1..510; bei DDP unbenutzt
    std::uint32_t ddpOffset{0};    // nur DDP: Byte-Offset des Stroms
    ColorSettings color;
};

/// Ein zusammenhängender Teil eines Fixtures innerhalb eines Slots.
struct OutputRun {
    std::uint32_t firstLed{0};     // Index in der LedMap
    std::uint32_t ledCount{0};
    std::uint32_t byteOffset{0};   // Position im Slot
    std::uint32_t encoder{0};      // Index in den Encodern des Plans
};

/// Ein Datenblock für ein Paket (Art-Net/sACN: ein Universe) oder einen Strom (DDP).
struct OutputSlot {
    Protocol protocol{Protocol::ArtNet};
    std::uint32_t destination{0};
    std::uint32_t address{0};  // Universe, bei DDP der Byte-Offset
    std::uint32_t length{0};   // Bytes bis zum letzten belegten Kanal
    std::vector<OutputRun> runs;
};

class OutputPlan {
public:
    /// Fehler: NotFound (Patch für ein Fixture, das die LedMap nicht enthält),
    /// AlreadyExists (zwei Patches für dasselbe Fixture, überlappende Kanäle),
    /// InvalidArgument (Universe oder Kanal außerhalb des Bereichs, Universe-Überlauf,
    /// ungültige Farbeinstellung).
    [[nodiscard]] static Result<OutputPlan> create(const mapping::LedMap& map,
                                                   std::span<const FixturePatch> patches);

    [[nodiscard]] const std::vector<OutputSlot>& slots() const noexcept { return slots_; }
    [[nodiscard]] std::size_t ledCount() const noexcept { return ledCount_; }

    /// Füllt pro Slot einen Puffer (Länge = slot.length, unbelegte Kanäle 0).
    /// `colors` muss genau ledCount() Einträge haben, sonst InvalidArgument.
    /// `out` behält seine Speicherblöcke zwischen Aufrufen, es gibt pro Frame
    /// keine neuen Allokationen, solange der Plan gleich bleibt.
    [[nodiscard]] Result<void> render(std::span<const ColorRgba> colors,
                                      std::vector<std::vector<std::uint8_t>>& out) const;

private:
    std::vector<OutputSlot> slots_;
    std::vector<ColorEncoder> encoders_;
    std::size_t ledCount_{0};
};

}  // namespace whitehole::output
