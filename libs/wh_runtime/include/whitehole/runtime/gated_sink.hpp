// gated_sink.hpp: ein Paketziel mit Schalter.
//
// Warum es diese Datei gibt: Die Oberfläche soll ein Projekt laufen lassen und anzeigen können,
// ohne dass sofort Pakete ins Netz gehen. Wer ein Projekt öffnet, dessen Controller gerade am
// Strom hängt, will nicht, dass der Effekt unbeabsichtigt Hardware ansteuert. Dieser Wrapper
// reicht Pakete nur weiter, solange er eingeschaltet ist; ausgeschaltet verwirft er sie und
// zählt mit. Das Umschalten ist jederzeit und von jedem Thread aus erlaubt.
#pragma once

#include <atomic>
#include <cstdint>

#include "whitehole/output/packet_sink.hpp"

namespace whitehole::runtime {

class GatedPacketSink final : public output::IPacketSink {
public:
    /// `inner` muss den Wrapper überleben. Startet ausgeschaltet.
    explicit GatedPacketSink(output::IPacketSink& inner) noexcept : inner_{inner} {}

    void setEnabled(bool enabled) noexcept { enabled_.store(enabled); }
    [[nodiscard]] bool enabled() const noexcept { return enabled_.load(); }
    /// Pakete, die im ausgeschalteten Zustand verworfen wurden.
    [[nodiscard]] std::uint64_t suppressed() const noexcept { return suppressed_.load(); }

    Result<void> send(const output::Endpoint& to, std::span<const std::uint8_t> packet) override {
        if (!enabled_.load()) {
            suppressed_.fetch_add(1);
            return {};
        }
        return inner_.send(to, packet);
    }

private:
    output::IPacketSink& inner_;
    std::atomic<bool> enabled_{false};
    std::atomic<std::uint64_t> suppressed_{0};
};

}  // namespace whitehole::runtime
