// dmx_store.hpp: der zuletzt empfangene Zustand aller DMX-Universen.
//
// Warum es das gibt: Der Empfänger (Netzwerk-Thread) und die Nodes (Engine-Thread) kennen einander
// nicht. Dazwischen liegt dieser Speicher: Der Empfänger schreibt das jeweils letzte Paket eines
// Universe hinein, ein Node liest einzelne Kanäle. Er hält zu jedem Universe die Zeit des letzten
// Pakets, damit ein Node "Signal weg" erkennen kann. Protokolle werden getrennt geführt: Art-Net
// Universe 1 und sACN Universe 1 sind verschiedene Universen.
//
// Threading: alle Methoden sind von jedem Thread aus erlaubt (ein Mutex; gelesen werden nur wenige Bytes).
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>

namespace whitehole::input {

enum class DmxProtocol : std::uint8_t { ArtNet = 0, Sacn = 1 };

inline constexpr std::size_t kDmxChannels = 512;

/// Ein gelesener Kanal (oder mehrere hintereinander).
struct DmxSample {
    std::array<std::uint8_t, 4> values{};  // bis zu vier Kanäle ab dem gewünschten
    double ageSeconds{0.0};                // seit dem letzten Paket dieses Universe
};

struct DmxStats {
    std::uint64_t packets{0};          // gespeicherte Pakete
    std::uint64_t ignored{0};          // verworfene (kein DMX, falscher Startcode, defekt)
    std::optional<DmxProtocol> lastProtocol;
    std::uint32_t lastUniverse{0};
};

class DmxStore {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    /// `now` ersetzt die Uhr (für Tests); Standard ist steady_clock.
    explicit DmxStore(Clock now = [] { return std::chrono::steady_clock::now(); }) : now_{std::move(now)} {}

    /// Speichert das Paket (höchstens 512 Kanäle, kürzere lassen den Rest unverändert auf 0).
    void write(DmxProtocol protocol, std::uint32_t universe, std::span<const std::uint8_t> channels);
    /// Zählt ein verworfenes Paket.
    void countIgnored();

    /// Liest `count` (1..4) Kanäle ab `firstChannel` (1..512). Leer, wenn für das Universe noch nie ein
    /// Paket kam oder die Kanäle außerhalb von 1..512 liegen.
    [[nodiscard]] std::optional<DmxSample> read(DmxProtocol protocol, std::uint32_t universe, std::uint32_t firstChannel,
                                                std::uint32_t count = 1) const;

    /// Die Universen, von denen irgendwann ein Paket kam, nach Protokoll.
    [[nodiscard]] std::map<std::uint32_t, double> universes(DmxProtocol protocol) const;

    [[nodiscard]] DmxStats stats() const;

private:
    struct Universe {
        std::array<std::uint8_t, kDmxChannels> data{};
        std::chrono::steady_clock::time_point updated;
    };

    Clock now_;
    mutable std::mutex mutex_;
    std::map<std::pair<std::uint8_t, std::uint32_t>, Universe> universes_;
    DmxStats stats_;
};

}  // namespace whitehole::input
