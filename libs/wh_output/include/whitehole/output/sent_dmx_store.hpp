// sent_dmx_store.hpp: der zuletzt gesendete Zustand aller Art-Net/sACN-Universen.
//
// Warum es das gibt: Gegenstück zu input::DmxStore (wh_input), nur für die Ausgabe statt den Eingang -
// für den DMX-Ausgangs-Monitor (Schritt 2 des DMX-Patch/Monitor-Gitters, siehe CLAUDE.md). OutputPipeline
// schreibt hier (optional, ein nullptr kostet nichts) direkt vor dem Senden die Rohbytes eines Slots
// hinein; die Studio-Oberfläche liest sie zur Anzeige. Bewusst ohne DDP: DDP adressiert Byte-Offsets eines
// Geräts, kein Universe-Konzept, und passt nicht in ein 512-Kanal-Gitter.
//
// Vereinfachung: Schlüssel ist (Protokoll, Universe), nicht zusätzlich das Ziel. Zwei Ziele mit derselben
// Universe-Nummer überschreiben sich dadurch gegenseitig im Monitor (zeigt dann, was zuletzt im Frame
// gesendet wurde) - für ein reines Anzeige-Werkzeug hinnehmbar, in der Praxis selten (ein Projekt hat meist
// ein Ziel je Netz).
//
// Threading: Pipeline schreibt auf ihrem eigenen Ausgabe-Thread, die Oberfläche liest auf dem UI-Thread;
// ein Mutex genügt, es werden nur wenige hundert Byte kopiert.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>

#include "whitehole/core/dmx_constants.hpp"
#include "whitehole/output/output_plan.hpp"  // Protocol

namespace whitehole::output {

inline constexpr std::size_t kSentDmxChannels = dmx::kChannelsPerUniverse;

/// Alle 512 Kanäle eines Universe, wie sie zuletzt gesendet wurden.
struct SentDmxFrame {
    std::array<std::uint8_t, kSentDmxChannels> channels{};
    double ageSeconds{0.0};  // seit dem letzten gesendeten Frame dieses Universe
};

class SentDmxStore {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    explicit SentDmxStore(Clock now = [] { return std::chrono::steady_clock::now(); }) : now_{std::move(now)} {}

    /// Merkt sich die gerade gesendeten Kanäle (höchstens 512 Kanäle, kürzere lassen den Rest unverändert
    /// auf 0 - wie input::DmxStore::write). `protocol` ist Art-Net oder sACN; DDP wird von der Pipeline gar
    /// nicht erst hereingegeben (siehe Kommentar oben).
    void write(Protocol protocol, std::uint32_t universe, std::span<const std::uint8_t> channels);

    /// Alle 512 Kanäle eines Universe in einer Sperre. Leer, wenn dafür noch nie gesendet wurde.
    [[nodiscard]] std::optional<SentDmxFrame> readAll(Protocol protocol, std::uint32_t universe) const;

    /// Die Universen, für die irgendwann gesendet wurde, nach Protokoll.
    [[nodiscard]] std::map<std::uint32_t, double> universes(Protocol protocol) const;

private:
    struct Universe {
        std::array<std::uint8_t, kSentDmxChannels> data{};
        std::chrono::steady_clock::time_point updated;
    };

    Clock now_;
    mutable std::mutex mutex_;
    std::map<std::pair<std::uint8_t, std::uint32_t>, Universe> universes_;
};

}  // namespace whitehole::output
