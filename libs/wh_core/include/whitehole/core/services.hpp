// services.hpp: Schnittstellen für Dependency Injection (Logger, Frame-Takt).
// Konkrete Implementierungen entstehen in späteren Schritten; die Verdrahtung
// passiert ausschließlich im Composition Root (main.cpp der Apps).
#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

namespace whitehole {

enum class LogLevel { Trace, Debug, Info, Warning, Error };

class ILogger {
public:
    virtual ~ILogger() = default;

    virtual void log(LogLevel level, std::string_view category, std::string_view message) = 0;
};

using Seconds = std::chrono::duration<double>;
using FrameIndex = std::uint64_t;

/// Zeitstempel eines Frames. Alle zeitabhängigen Nodes lesen ausschließlich
/// diesen Wert und nie die Systemuhr, damit Ergebnisse reproduzierbar sind.
struct FrameTime {
    FrameIndex frame{0};
    Seconds time{0.0};
    Seconds delta{0.0};
};

/// Taktgeber des Renderloops. Die Echtzeit-Variante läuft mit fester Rate;
/// Tests und Offline-Rendering (Bake) nutzen eine manuell fortgeschriebene Uhr.
class IFrameClock {
public:
    virtual ~IFrameClock() = default;

    [[nodiscard]] virtual FrameTime current() const = 0;
    virtual FrameTime advance() = 0;
};

}  // namespace whitehole
