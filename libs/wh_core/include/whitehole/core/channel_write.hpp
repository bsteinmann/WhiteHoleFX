// channel_write.hpp: ein Block DMX-Kanalwerte, den ein Node je Frame an die Ausgabe gibt.
//
// Warum es diese Datei gibt: Die DMX-Output-Nodes (wh.output.dmx, wh.output.dmxcolor) schreiben Werte aus dem
// Node-Graph auf Kanäle eines Universes, unabhängig von LEDs. Der Weg läuft durch mehrere Bibliotheken (Kernel ->
// ValueStore -> Session -> Frame -> Ausgabe-Pipeline); der Typ liegt darum in wh_core, wo ihn alle sehen, ohne dass
// wh_values von wh_output abhängt. `destination` ist die Controller-Nummer des Projekts (wie bei FixturePatch), das
// Protokoll (Art-Net oder sACN) kommt vom Controller. `firstChannel` zählt ab 1; `data` sind die Bytes ab dort.
#pragma once

#include <cstdint>
#include <vector>

namespace whitehole {

struct ChannelWrite {
    std::uint32_t destination{0};
    std::uint32_t universe{0};
    std::uint16_t firstChannel{1};
    std::vector<std::uint8_t> data;

    bool operator==(const ChannelWrite&) const = default;
};

}  // namespace whitehole
