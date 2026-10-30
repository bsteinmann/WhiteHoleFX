// render_stage.hpp: Einhängepunkt für alles, was pro Frame auf der GPU läuft.
#pragma once

#include "whitehole/core/services.hpp"
#include "whitehole/gpu/gpu_device.hpp"

namespace whitehole::engine {

/// Ein Abschnitt der Frame-Berechnung, z. B. der Graph-Executor (Schritt 4),
/// der Mapping-Pass (Schritt 5) oder der Ausgabe-Pass. Die Engine ruft die
/// Stages in der eingestellten Reihenfolge auf; jede zeichnet ihre Befehle
/// in den gemeinsamen Recorder des Frames auf.
class IRenderStage {
public:
    virtual ~IRenderStage() = default;

    virtual void record(const FrameTime& time, gpu::ICommandRecorder& recorder) = 0;
};

}  // namespace whitehole::engine
