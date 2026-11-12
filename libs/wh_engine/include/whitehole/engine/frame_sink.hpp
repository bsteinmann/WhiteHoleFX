// frame_sink.hpp: Übergabepunkt für fertig berechnete Frames.
#pragma once

#include <cstddef>
#include <vector>

#include "whitehole/core/channel_write.hpp"
#include "whitehole/core/services.hpp"

namespace whitehole::engine {

/// Ein fertig berechneter Frame auf der CPU: der Inhalt des Ausgabepuffers der GPU.
/// Bei der Abtast-Stage sind das 16 Byte je LED (lineares RGBA, vier f32), eventuell mit
/// Reserve dahinter. Farbreihenfolge, Gamma und Paketformat wendet erst wh_output an.
struct PixelFrame {
    FrameIndex frame{0};
    std::vector<std::byte> bytes;
    /// DMX-Kanäle der Output-Nodes dieses Frames (wh.output.dmx); die Session hängt sie an, die Ausgabe sendet sie.
    std::vector<ChannelWrite> channels;
};

/// Empfänger fertiger Frames (z. B. der Output-Thread). publish() wird auf dem
/// Engine-Thread aufgerufen und muss schnell zurückkehren.
class IFrameSink {
public:
    virtual ~IFrameSink() = default;

    virtual void publish(PixelFrame frame) = 0;
};

}  // namespace whitehole::engine
