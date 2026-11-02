// master_dmx.hpp: ein DMX-Kanal, der die Gesamthelligkeit (Master) steuert.
//
// Warum es diese Datei gibt (Issue #17): Am Lichtpult soll ein Fader den Master von WhiteHoleFX fahren. Welcher
// Kanal das ist, hängt vom Auftritt ab (Pult, Universe), nicht vom Projekt; die Oberfläche merkt es sich bei den
// Programmeinstellungen und reicht es dem Player. Der Player liest den Wert aus dem DMX-Speicher der Bibliothek
// (derselbe, aus dem die DMX-Nodes lesen) und setzt damit setMasterBrightness.
#pragma once

#include <cstdint>

#include "whitehole/input/dmx_store.hpp"

namespace whitehole::runtime {

struct MasterDmxSource {
    input::DmxProtocol protocol{input::DmxProtocol::ArtNet};
    std::uint32_t universe{0};  // wie im Protokoll (Art-Net: Port-Adresse, sACN: Universe)
    std::uint32_t channel{1};   // 1..512

    bool operator==(const MasterDmxSource&) const = default;
};

/// Bleibt das letzte Paket des Universe länger aus, hält der Master seinen Wert (der Regler am Pult ist weg,
/// das Licht soll nicht plötzlich dunkel oder hell werden) und der Regler in der Oberfläche ist wieder frei.
inline constexpr double kMasterDmxTimeoutSeconds = 5.0;

}  // namespace whitehole::runtime
