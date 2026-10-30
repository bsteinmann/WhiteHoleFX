// packet_sink.hpp: die Schnittstelle zwischen fertigen Paketen und dem Netzwerk.
//
// Warum es diese Datei gibt: Die Pipeline soll Pakete bauen, ohne zu wissen, wie sie
// verschickt werden. Hinter dieser Schnittstelle steckt im Betrieb ein UDP-Socket
// (udp_sender.hpp), im Test ein Rekorder, der die Bytes nur sammelt. So lässt sich die
// gesamte Ausgabe prüfen, ohne ein Netzwerk zu benutzen. Das ist dasselbe Muster wie
// IGpuDevice im GPU-Teil: die Logik hängt an einer Schnittstelle, nicht an der Hardware.
//
// Aufgabe: ein Paket an eine Adresse schicken. Der Aufruf kommt vom Ausgabe-Thread und
// darf nicht lange blockieren; ein Fehler betrifft nur dieses eine Paket.
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "whitehole/core/result.hpp"

namespace whitehole::output {

/// Ziel eines Pakets: IPv4-Adresse als Text ("192.168.1.50") und UDP-Port.
struct Endpoint {
    std::string address;
    std::uint16_t port{0};

    bool operator==(const Endpoint&) const = default;
};

class IPacketSink {
public:
    virtual ~IPacketSink() = default;

    [[nodiscard]] virtual Result<void> send(const Endpoint& to, std::span<const std::uint8_t> packet) = 0;
};

}  // namespace whitehole::output
