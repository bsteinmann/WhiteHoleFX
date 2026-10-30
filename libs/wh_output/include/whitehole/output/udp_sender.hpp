// udp_sender.hpp: verschickt Pakete über einen UDP-Socket.
//
// Warum es diese Datei gibt: Sie ist der einzige Ort im Modul, der das Betriebssystem
// berührt. Alles davor (Plan, Encoder, Pipeline) bleibt reine Rechnung und ist ohne
// Netzwerk testbar; hier steckt nur das Senden.
//
// Aufgabe: ein IPv4-UDP-Socket, nicht blockierend. Ist der Sendepuffer des Systems
// voll, geht das Paket verloren und send() meldet einen Fehler; Warten würde den
// Ausgabe-Thread aufhalten, und ein altes Bild ist wertlos. Broadcast ist erlaubt
// (Art-Net), Multicast mit TTL 1 (sACN, bleibt im lokalen Netz).
//
// Adressen werden nur als IPv4-Zahlen akzeptiert und je Ziel einmal umgewandelt und
// gemerkt, damit pro Paket kein Lookup anfällt.
//
// Plattform: POSIX-Sockets (Linux, macOS) und Winsock (Windows). Der Windows-Zweig ist
// geschrieben, aber noch nicht auf Windows übersetzt oder getestet; das prüft der CI-Job
// "Windows".
// Threads: send() von einem Thread aus.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "whitehole/output/packet_sink.hpp"

namespace whitehole::output {

class UdpSender final : public IPacketSink {
public:
    /// Fehler: IoError (Socket oder Winsock nicht startbar).
    [[nodiscard]] static Result<std::unique_ptr<UdpSender>> create();

    ~UdpSender() override;
    UdpSender(const UdpSender&) = delete;
    UdpSender& operator=(const UdpSender&) = delete;

    /// Fehler: InvalidArgument (keine IPv4-Zahl), IoError (Senden fehlgeschlagen).
    [[nodiscard]] Result<void> send(const Endpoint& to, std::span<const std::uint8_t> packet) override;

private:
    struct Impl;
    explicit UdpSender(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

}  // namespace whitehole::output
