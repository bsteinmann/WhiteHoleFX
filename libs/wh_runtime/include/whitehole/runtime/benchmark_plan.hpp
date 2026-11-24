// benchmark_plan.hpp: welche Stufen der Benchmark misst, in welcher Reihenfolge, und die Kennzahlen einer Stufe.
//
// Warum es diese Datei gibt (Entwurf docs/entwurf-benchmark.md, Schritt 1): Ob eine Kombination aus Auflösung, LED-Zahl
// und Effekt auf einer Grafikkarte flüssig läuft, soll gemessen statt geraten werden. Der Plan ist reine Rechnung ohne
// GPU und ohne Qt, damit er lokal und in der CI testbar ist; Studio-Knopf und `whitehole_cli --benchmark` nutzen ihn
// gleich.
//
// Zwei getrennte Messreihen (Entscheidung 24.11.): Projektion (Textur in Zielauflösung, offscreen) und LEDs (ein
// Abtastpunkt je LED), wählbar einzeln oder beide nacheinander, kein Kreuzprodukt. Innerhalb einer Reihe geht es vom
// kleinsten zum größten Aufwand, damit die Grenze früh auffällt und größere Stufen übersprungen werden können.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace whitehole::runtime {

enum class BenchmarkSeries { Projection, Leds, Both };
enum class BenchmarkMode { Quick, Full };
enum class BenchmarkKind { Projection, Leds };
enum class BenchmarkScene { Plasma, Particles, Maze, Layers };

struct BenchmarkStage {
    BenchmarkKind kind{BenchmarkKind::Leds};
    BenchmarkScene scene{BenchmarkScene::Plasma};
    std::uint32_t width{0};   // nur Projektion
    std::uint32_t height{0};  // nur Projektion
    std::uint64_t leds{0};    // nur LEDs

    /// Zahl der Abtastpunkte (Pixel bzw. LEDs): Maß für den Aufwand, Grundlage der Hochrechnung.
    [[nodiscard]] std::uint64_t points() const noexcept {
        return kind == BenchmarkKind::Projection ? std::uint64_t{width} * height : leds;
    }
    bool operator==(const BenchmarkStage&) const = default;
};

/// Voller Lauf: Projektion 480×270, 960×540, 1920×1080; LEDs 1 000, 10 000, 100 000, 1 Mio.; je vier Szenen. Reihenfolge je
/// Reihe nach Größe, innerhalb einer Größe nach Szene. Schnelltest: drei Stufen mit der Szene „Schichten“ (die kleinsten
/// drei Größen der gewählten Reihe; bei beiden Reihen Projektion 960×540, LEDs 10 000 und 100 000).
[[nodiscard]] std::vector<BenchmarkStage> benchmarkPlan(BenchmarkSeries series, BenchmarkMode mode);

/// Kennzeichen für Tabellen und Dateien, unabhängig von der Sprache: "plasma", "particles", "maze", "layers".
[[nodiscard]] const char* benchmarkSceneId(BenchmarkScene scene) noexcept;
/// "1920x1080" bzw. "100000 LEDs".
[[nodiscard]] std::string benchmarkStageLabel(const BenchmarkStage& stage);

/// Zeit eines Frames: CPU (Werte, Aufzeichnen, Einreichen), GPU (Zeitstempel, falls der Adapter sie kann) und gesamt
/// (Einreichen bis fertig zurückgelesen).
struct BenchmarkFrame {
    double cpuMs{0.0};
    std::optional<double> gpuMs;
    double totalMs{0.0};
};

struct BenchmarkStats {
    std::size_t frames{0};
    double medianMs{0.0};
    double p95Ms{0.0};
    double maxMs{0.0};
    double cpuMedianMs{0.0};
    std::optional<double> gpuMedianMs;  // nur, wenn jeder Frame eine GPU-Zeit hat
};

/// Median und 95. Perzentil (Rangverfahren) der Gesamtzeit, dazu CPU- und GPU-Median. Leere Eingabe: alles 0.
[[nodiscard]] BenchmarkStats summarizeFrames(std::span<const BenchmarkFrame> frames);

}  // namespace whitehole::runtime
