// benchmark_sensors.hpp: GPU-Temperatur und -Takt während des Benchmarks (erstmal nur NVIDIA).
//
// Warum es diese Datei gibt (Entwurf docs/entwurf-benchmark.md, Entscheidung B vom 24.11.): Bricht eine Karte nach
// einigen Minuten ein, weil sie heiß wird und den Takt senkt, soll man das in der Tabelle sehen. WebGPU kennt keine
// Sensoren; NVIDIA liefert sie über NVML. Die Bibliothek wird zur Laufzeit geladen (libnvidia-ml.so.1 bzw. nvml.dll), es
// gibt also keine Abhängigkeit beim Bauen und keine auf Rechnern ohne NVIDIA-Treiber. Andere Hersteller: Spalten leer.
#pragma once

#include <memory>
#include <optional>
#include <string_view>

namespace whitehole::runtime {

struct GpuSensorReading {
    std::optional<unsigned> temperatureC;
    std::optional<unsigned> graphicsClockMHz;
};

class IGpuSensors {
public:
    virtual ~IGpuSensors() = default;
    [[nodiscard]] virtual GpuSensorReading read() = 0;
};

/// NVML-Sensor für die NVIDIA-Karte, deren Name zu `deviceName` passt (der Adapter, auf dem gemessen wird; bei Rechnern
/// mit Intel- und NVIDIA-Grafik soll nicht die falsche Karte berichtet werden). nullptr ohne NVML oder ohne passende Karte.
[[nodiscard]] std::unique_ptr<IGpuSensors> openNvidiaSensors(std::string_view deviceName);

/// Passt ein NVML-Name zum Adapternamen? Gleich bis auf Groß-/Kleinschreibung, oder einer enthält den anderen
/// (NVML: "NVIDIA GeForce GTX 1060 6GB", Vulkan: "NVIDIA GeForce GTX 1060 6GB" oder ohne Zusatz). Reine Rechnung.
[[nodiscard]] bool gpuNamesMatch(std::string_view nvmlName, std::string_view adapterName) noexcept;

}  // namespace whitehole::runtime
