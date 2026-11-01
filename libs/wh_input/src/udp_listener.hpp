// udp_listener.hpp (intern): ein IPv4-UDP-Socket zum Empfangen, mit Zeitlimit und Multicast-Beitritt.
//
// Warum es das gibt: Die einzige Stelle im Modul, die das Betriebssystem berührt; die Implementierung
// steht in udp_listener.cpp, getrennt für POSIX und Winsock. Nicht Teil der öffentlichen Schnittstelle.
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "whitehole/core/result.hpp"

namespace whitehole::input {

class UdpListener {
public:
    /// Bindet 0.0.0.0:port (0 = freier Port), mit Adress-Wiederverwendung. Gebunden wird immer an alle
    /// Adressen (Art-Net-Broadcast käme an eine einzelne Adresse nicht an). `interfaceAddress` (IPv4 einer
    /// Netzwerkkarte, leer = Wahl des Systems) bestimmt nur, über welche Karte Multicast-Gruppen
    /// beigetreten wird. Fehler: InvalidArgument (keine IPv4-Zahl), IoError.
    [[nodiscard]] static Result<std::unique_ptr<UdpListener>> open(std::uint16_t port,
                                                                    const std::string& interfaceAddress = {});

    ~UdpListener();
    UdpListener(const UdpListener&) = delete;
    UdpListener& operator=(const UdpListener&) = delete;

    /// Wartet höchstens `timeoutMs` auf ein Paket. Liefert die Länge, 0 bei Zeitablauf, -1 bei Fehler.
    int receive(std::span<std::uint8_t> buffer, int timeoutMs);

    [[nodiscard]] std::uint16_t port() const;

    /// Tritt der sACN-Multicast-Gruppe 239.255.hi.lo bei bzw. verlässt sie. true bei Erfolg.
    bool joinSacnGroup(std::uint32_t universe);
    bool leaveSacnGroup(std::uint32_t universe);

private:
    struct Impl;
    explicit UdpListener(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

}  // namespace whitehole::input
