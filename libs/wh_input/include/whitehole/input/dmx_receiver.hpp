// dmx_receiver.hpp: empfängt Art-Net und sACN über UDP und schreibt die Kanalwerte in einen DmxStore.
//
// Warum es das gibt: Der Teil mit dem Betriebssystem. Je Protokoll ein UDP-Socket und ein Thread, der
// Pakete liest, mit parseArtNet/parseSacn auswertet und in den Speicher legt. Fremde oder defekte
// Pakete werden gezählt und verworfen. Der Empfänger blockiert nie den Rest der Anwendung: jeder
// Thread wartet höchstens 100 ms und prüft dann, ob er beenden soll.
//
// Zuhören ist eine Entscheidung des Menschen (Netzwerk-Port öffnen, Firewall-Rückfrage unter Windows):
// ein neu erzeugter Empfänger tut nichts, erst start() öffnet die Sockets.
//
// sACN: Pakete gehen an die Multicast-Adresse des Universe; sie kommen nur an, wenn der Empfänger der
// Gruppe beigetreten ist. setSacnUniverses() tritt bei den gewünschten Universen bei und verlässt
// die übrigen (auf der gewählten Netzwerkkarte, sonst der Standardschnittstelle des Systems). Unicast-Pakete kommen immer an.
//
// Plattform: POSIX-Sockets und Winsock. Der Windows-Zweig ist noch nicht auf Windows übersetzt; das
// prüft der CI-Job "Windows".
// Threads: start/stop/setSacnUniverses vom Hauptthread; der Speicher ist threadsicher.
#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>

#include "whitehole/core/dmx_constants.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/input/dmx_store.hpp"

namespace whitehole::input {

inline constexpr std::uint16_t kArtNetListenPort = dmx::kArtNetPort;
inline constexpr std::uint16_t kSacnListenPort = dmx::kSacnPort;

struct DmxReceiverConfig {
    bool artnet{true};
    bool sacn{true};
    /// 0 = das System sucht einen freien Port (für Tests). Standard sind die Protokoll-Ports.
    std::uint16_t artnetPort{kArtNetListenPort};
    std::uint16_t sacnPort{kSacnListenPort};
    /// IPv4-Adresse der Netzwerkkarte, über die sACN-Multicast-Gruppen beigetreten wird (leer = Wahl des
    /// Systems). Gehört zur Wahl "Netzwerkkarte" in den Einstellungen. Art-Net-Broadcast und Unicast kommen
    /// weiterhin von jeder Karte an.
    std::string interfaceAddress;
};

class DmxReceiver {
public:
    explicit DmxReceiver(std::shared_ptr<DmxStore> store);
    ~DmxReceiver();
    DmxReceiver(const DmxReceiver&) = delete;
    DmxReceiver& operator=(const DmxReceiver&) = delete;

    /// Öffnet die Sockets und startet die Threads. Schon laufend: erst stop(). Schlägt ein Socket fehl
    /// (Port belegt), läuft nichts und der Fehler nennt das Protokoll. Fehler: IoError.
    [[nodiscard]] Result<void> start(const DmxReceiverConfig& config);
    void stop();
    [[nodiscard]] bool running() const;

    /// Die tatsächlich gebundenen Ports (0 = Protokoll aus).
    [[nodiscard]] std::uint16_t artnetPort() const;
    [[nodiscard]] std::uint16_t sacnPort() const;

    /// Universen, deren Multicast-Gruppe beigetreten sein soll. Wirkt sofort, sobald sACN läuft, und
    /// wird bei einem späteren start() erneut angewendet.
    void setSacnUniverses(const std::set<std::uint32_t>& universes);
    /// Wie viele Beitritte das System abgelehnt hat (z. B. keine Netzwerkschnittstelle).
    [[nodiscard]] std::size_t failedJoins() const;

    [[nodiscard]] const std::shared_ptr<DmxStore>& store() const { return store_; }

private:
    struct Impl;
    std::shared_ptr<DmxStore> store_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace whitehole::input
