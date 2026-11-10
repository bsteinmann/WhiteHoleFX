// pong_sim.hpp: die Spiellogik von Pong, ohne Node, GPU und Oberfläche.
//
// Warum es diese Datei gibt: Bild-Nodes sind zustandslos, ein Spiel braucht aber Gedächtnis (Ball, Schläger,
// Punkte). Der Rechen-Node "Pong" (builtin_pong.cpp) hält darum eine PongSim und gibt ihren Stand als Werte
// aus; der Bild-Node "Pong Display" zeichnet sie. Die Simulation selbst kennt nur Zahlen, damit Tests sie
// ohne Evaluator durchspielen können.
//
// Spielfeld: x und y laufen von 0 bis 1 (y = 0 unten). Die Schläger stehen bei x = margin und x = 1 - margin
// und sind `paddleSize` lang (Anteil der Höhe). Gerechnet wird in festen Schritten von 1/240 s, damit das Spiel
// bei jeder Bildrate gleich verläuft. Zufall kommt aus einem eigenen, mit `seed` startenden Erzeuger: gleiche
// Einstellungen und gleicher Seed ergeben dasselbe Spiel.
#pragma once

#include <cstdint>

namespace whitehole::nodes::detail {

struct PongSettings {
    int leftControl{0};   // 0 = Rechner spielt, 1 = Schläger folgt `leftInput`
    int rightControl{0};
    double leftInput{0.5};
    double rightInput{0.5};
    double speed{0.6};            // Feldbreiten pro Sekunde
    bool beatMode{false};         // Überquerung dauert `beatsPerCrossing` Schläge, Aufschlag auf dem Schlag
    double bpm{120.0};
    double beatsPerCrossing{2.0};
    double speedUp{0.05};         // Zuwachs des Tempos je Schlägertreffer (nicht im Taktmodus)
    double paddleSize{0.25};
    double ballRadius{0.03};
    double margin{0.05};
    double skill{0.85};           // Chance (0..1), dass der Rechner den Ball erreicht
    double paddleSpeed{1.2};      // Höhen pro Sekunde, die ein Rechner-Schläger schafft
    double maxAngle{0.9};         // größter Abgangswinkel in Bogenmaß (Treffer am Schlägerrand)
    int maxScore{7};
    double serveDelay{0.8};
    bool autoServe{true};
    double winHold{2.0};          // so lange bleibt der Gewinner stehen
    bool restart{true};           // danach ein neues Spiel beginnen
    std::uint64_t seed{1};
};

/// Was seit dem letzten step() passiert ist (nur "war da", keine Anzahl).
struct PongEvents {
    bool paddleHit{false};
    bool wallHit{false};
    bool point{false};
};

struct PongView {
    double ballX{0.5};
    double ballY{0.5};
    double left{0.5};
    double right{0.5};
    int scoreLeft{0};
    int scoreRight{0};
    int winner{0};        // 0 = keiner, 1 = links, 2 = rechts (während der Siegerpause)
    double flash{1.0};    // blinkt in der Siegerpause zwischen 1 und 0,25
    bool flying{false};   // der Ball ist unterwegs
};

class PongSim {
public:
    /// Schreibt die Zeit `dt` (Sekunden) fort. `serve`: Aufschlag jetzt, wenn der Ball wartet (steigende Flanke);
    /// `beat`: in diesem Frame fällt ein Schlag (für den Aufschlag im Taktmodus).
    PongView step(double dt, const PongSettings& settings, bool serve, bool beat, PongEvents& events);

    [[nodiscard]] PongView view() const;

private:
    enum class Phase { Waiting, Flying, GameOver };

    void substep(double dt, const PongSettings& s, bool serve, bool beat, PongEvents& events);
    void launch(const PongSettings& s);
    void planLeg(const PongSettings& s);
    void movePaddles(double dt, const PongSettings& s);
    void flight(double dt, const PongSettings& s, PongEvents& events);
    void score(int side, const PongSettings& s, PongEvents& events);
    double uniform();

    Phase phase_{Phase::Waiting};
    double ballX_{0.5};
    double ballY_{0.5};
    double vx_{0.0};
    double vy_{0.0};
    double speedNow_{0.0};
    double left_{0.5};
    double right_{0.5};
    double leftTarget_{0.5};
    double rightTarget_{0.5};
    int scoreLeft_{0};
    int scoreRight_{0};
    int winner_{0};
    int serveToward_{1};  // +1 = nach rechts, -1 = nach links
    double timer_{0.0};
    double accumulator_{0.0};
    std::uint64_t rng_{0};
    std::uint64_t seededWith_{0};
    bool seeded_{false};
};

}  // namespace whitehole::nodes::detail
