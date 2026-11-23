// dmx_learn.hpp: "Lernen" für DMX - den Kanal finden, den man gerade am Pult bewegt.
//
// Warum es das gibt: Statt Protokoll, Universe und Kanal von Hand einzutippen, drückt man "Lernen" und schiebt am Pult
// den gewünschten Regler. Gelernt wird die Änderung seit dem Scharfstellen, nicht der Wert: Beim Scharfstellen merkt sich
// DmxLearn den Stand aller empfangenen Universen; poll() meldet den Kanal, der sich seitdem am stärksten bewegt hat
// (mindestens kMinChange, damit Rauschen eines Pults nichts auslöst). Gegenstück zu midi::MidiLearn.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "whitehole/input/dmx_store.hpp"

namespace whitehole::input {

struct DmxAddress {
    DmxProtocol protocol{DmxProtocol::ArtNet};
    std::uint32_t universe{0};
    std::uint32_t channel{1};  // 1..512
    bool operator==(const DmxAddress&) const = default;
};

class DmxLearn {
public:
    static constexpr int kMinChange = 8;

    explicit DmxLearn(std::shared_ptr<const DmxStore> store) : store_{std::move(store)} {}

    void arm();
    void cancel() noexcept { armed_ = false; }
    [[nodiscard]] bool armed() const noexcept { return armed_; }
    /// Der Kanal mit der größten Änderung seit arm() (mindestens kMinChange); beendet dann das Lernen.
    /// Ein Universe, das erst nach arm() auftaucht, zählt gegen 0.
    [[nodiscard]] std::optional<DmxAddress> poll();

private:
    using Key = std::pair<std::uint8_t, std::uint32_t>;  // Protokoll, Universe
    std::shared_ptr<const DmxStore> store_;
    std::map<Key, std::array<std::uint8_t, kDmxChannels>> baseline_;
    bool armed_{false};
};

}  // namespace whitehole::input
