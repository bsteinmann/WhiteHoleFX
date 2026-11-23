// midi_store.hpp: der zuletzt empfangene MIDI-Zustand (Regler, Noten) plus eine Ereignis-Warteschlange.
//
// Warum es das gibt: Das Gerät meldet sich auf einem eigenen Thread, die Nodes und der Live-Modus lesen auf dem
// Engine-Thread. Dazwischen liegt dieser Speicher, wie beim DmxStore: Der Empfänger schreibt, andere lesen den Stand
// ("wie weit steht Regler 48?") und können "Signal weg" über das Alter erkennen. Zusätzlich gibt es eine begrenzte
// Ereignisliste für Dinge, die nicht als Zustand taugen: ein Pad-Druck muss genau einmal wirken, auch wenn die Taste
// schon wieder losgelassen ist, bevor der Engine-Thread nachsieht.
//
// Threading: alle Methoden sind von jedem Thread aus erlaubt (ein Mutex; es werden nur wenige Bytes bewegt).
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

#include "whitehole/midi/midi_message.hpp"

namespace whitehole::midi {

struct CcSample {
    std::uint8_t value{0};
    double ageSeconds{0.0};  // seit der letzten Nachricht für diesen Regler
};

struct NoteSample {
    bool on{false};
    std::uint8_t velocity{0};  // Velocity des letzten Note-On (bleibt nach dem Loslassen stehen)
    double ageSeconds{0.0};    // seit der letzten Nachricht für diese Note
};

struct LastMessage {
    MidiMessage message;
    double ageSeconds{0.0};
};

struct MidiStats {
    std::uint64_t messages{0};  // angenommene Nachrichten
    std::uint64_t dropped{0};   // aus der Ereignisliste verdrängt, weil niemand sie abholte
};

class MidiStore {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;
    /// Mehr nicht abgeholte Ereignisse werden gehalten; ältere fallen heraus (und zählen in MidiStats::dropped).
    static constexpr std::size_t kMaxQueuedEvents = 256;

    /// `now` ersetzt die Uhr (für Tests); Standard ist steady_clock.
    explicit MidiStore(Clock now = [] { return std::chrono::steady_clock::now(); }) : now_{std::move(now)} {}

    /// Übernimmt eine Nachricht in den Zustand und die Ereignisliste.
    void apply(const MidiMessage& message);

    /// Letzter Wert eines Reglers (Kanal 0..15, CC 0..127). Leer, wenn nie einer kam.
    [[nodiscard]] std::optional<CcSample> readCc(std::uint8_t channel, std::uint8_t controller) const;
    /// Zustand einer Note. Leer, wenn für sie nie eine Nachricht kam.
    [[nodiscard]] std::optional<NoteSample> readNote(std::uint8_t channel, std::uint8_t note) const;
    /// Die zuletzt angenommene Nachricht (für die Anzeige in den Einstellungen).
    [[nodiscard]] std::optional<LastMessage> last() const;

    /// Holt alle seit dem letzten Aufruf eingegangenen Ereignisse in der Reihenfolge ihres Eintreffens ab.
    [[nodiscard]] std::vector<MidiMessage> takeEvents();

    [[nodiscard]] MidiStats stats() const;

private:
    struct Slot {
        bool seen{false};
        bool on{false};
        std::uint8_t value{0};
        std::chrono::steady_clock::time_point updated;
    };

    [[nodiscard]] static std::size_t index(std::uint8_t channel, std::uint8_t number) noexcept {
        return static_cast<std::size_t>(channel & 0x0F) * 128 + (number & 0x7F);
    }

    Clock now_;
    mutable std::mutex mutex_;
    std::array<Slot, 16 * 128> controllers_{};
    std::array<Slot, 16 * 128> notes_{};
    std::deque<MidiMessage> events_;
    std::optional<MidiMessage> last_;
    std::chrono::steady_clock::time_point lastTime_;
    MidiStats stats_;
};

}  // namespace whitehole::midi
