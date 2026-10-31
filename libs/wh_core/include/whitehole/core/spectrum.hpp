// spectrum.hpp: ein Audio-Spektrum fester Größe, wie es Nodes und Shader austauschen.
//
// Warum es diese Datei gibt: Der Feldcompiler (wh_compiler), der Werte-Graph (wh_graph/wh_values)
// und die Audio-Analyse (wh_audio) müssen dieselbe Anzahl Bänder meinen, kennen einander aber nicht.
// Das Spektrum liegt deshalb hier in wh_core. 32 Bänder sind ein Kompromiss: genug für einen
// lesbaren Equalizer auf einer Matrix, klein genug für den Uniform-Puffer (8 × vec4 = 128 Bytes).
// Die Bänder sind logarithmisch von 40 Hz bis 16 kHz verteilt (siehe wh_audio/analyzer.hpp).
// Abhängigkeiten: keine.
#pragma once

#include <array>
#include <cstddef>

namespace whitehole {

inline constexpr std::size_t kSpectrumBands = 32;

/// Energie je Band, 0..1 (0 = still, 1 = Vollaussteuerung). Dieselbe Struktur trägt auch die 32 Punkte
/// einer Wellenform (Porttyp AudioWave), dann mit Werten von -1 bis 1.
struct Spectrum {
    std::array<float, kSpectrumBands> bands{};
};

}  // namespace whitehole
