// audio_usage.hpp: ob ein Projekt Audio-Nodes enthält.
//
// Warum es das gibt: Das Mikrofon (oder die Datei) läuft nur auf ausdrücklichen Wunsch. Die
// Oberfläche und der Konsolen-Runner können deshalb anzeigen "dieses Projekt erwartet Audio, der
// Eingang ist aber aus" und den Bericht (--check) kann es erwähnen.
#pragma once

#include "whitehole/graph/graph_model.hpp"

namespace whitehole::runtime {

struct AudioUsage {
    int nodes{0};  // Zahl der Nodes, die aus dem Audio-Eingang lesen
    [[nodiscard]] bool any() const { return nodes > 0; }
};

/// Zählt die Nodes wh.value.audiolevel, audioband, audiospectrum, audiowave und beat. (Tap Tempo braucht kein
/// Audio, Spectrum Bars bekommt sein Spektrum über eine Verbindung.)
[[nodiscard]] AudioUsage findAudioUsage(const graph::Graph& graph);

}  // namespace whitehole::runtime
