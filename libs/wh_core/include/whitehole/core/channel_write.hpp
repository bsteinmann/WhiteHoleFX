// channel_write.hpp: ein Block DMX-Kanalwerte, den ein Node je Frame an die Ausgabe gibt.
//
// Warum es diese Datei gibt: Die DMX-Output-Nodes (wh.output.dmx, wh.output.dmxcolor) schreiben Werte aus dem
// Node-Graph auf Kanäle, unabhängig von LEDs. Der Weg läuft durch mehrere Bibliotheken (Kernel -> ValueStore ->
// Session -> Frame -> Ausgabe-Pipeline); der Typ liegt darum in wh_core, wo ihn alle sehen, ohne dass wh_values
// von wh_output abhängt.
//
// Ein Node kennt keinen Controller, kein Universe und keinen echten DMX-Kanal mehr (das wäre Betriebswissen,
// das im Node-Graph nicht hingehört). Stattdessen schreibt er in einen internen, projektweiten Kanal-Pool
// (1..512, wie ein zusätzliches virtuelles Universe): `poolChannel` ist der erste Pool-Kanal (ab 1), `data` die
// Bytes ab dort. Erst `output::DmxChannelPatch` (wh_output, Projektdatei-Anweisung `dmxpatch`) bildet Bereiche
// dieses Pools auf echte Controller/Universe/Kanal ab; unpatchte Pool-Kanäle werden einfach nicht gesendet
// (wie ein ungepatchtes Fixture).
#pragma once

#include <cstdint>
#include <vector>

namespace whitehole {

struct ChannelWrite {
    std::uint16_t poolChannel{1};
    std::vector<std::uint8_t> data;

    bool operator==(const ChannelWrite&) const = default;
};

}  // namespace whitehole
