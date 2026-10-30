// frame_sink.hpp: Übergabepunkt für fertig berechnete Frames.
#pragma once

#include <cstddef>
#include <vector>

#include "whitehole/core/services.hpp"

namespace whitehole::engine {

/// Ein fertig berechneter Frame auf der CPU: die Ausgabebytes aller LEDs
/// (bereits in Farbreihenfolge und Gamma des Ausgabe-Passes).
struct PixelFrame {
    FrameIndex frame{0};
    std::vector<std::byte> bytes;
};

/// Empfänger fertiger Frames (z. B. der Output-Thread). publish() wird auf dem
/// Engine-Thread aufgerufen und muss schnell zurückkehren.
class IFrameSink {
public:
    virtual ~IFrameSink() = default;

    virtual void publish(PixelFrame frame) = 0;
};

}  // namespace whitehole::engine
