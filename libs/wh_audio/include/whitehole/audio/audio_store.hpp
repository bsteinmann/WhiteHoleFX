// audio_store.hpp: der zuletzt berechnete Audio-Zustand, geteilt zwischen Audio-Thread und Nodes.
//
// Warum es diese Datei gibt: Genau wie beim DMX-Eingang kennen Quelle (Audio-Thread) und Nodes
// (Engine-Thread) einander nicht. Dazwischen liegt dieser Speicher: Die Quelle schreibt nach jedem
// Analyseschritt den neuesten `AudioFrame` hinein, ein Node liest ihn einmal pro Bild. Der Speicher
// merkt sich auch, wann zuletzt geschrieben wurde, damit ein Node "kein Signal" erkennen kann, und
// einen Beschreibungstext der Quelle für die Statusanzeige.
//
// Threading: alle Methoden sind von jedem Thread aus erlaubt (ein Mutex; ein Frame ist ~200 Bytes).
#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include "whitehole/audio/analyzer.hpp"

namespace whitehole::audio {

struct AudioSnapshot {
    AudioFrame frame;
    double ageSeconds{0.0};  // seit dem letzten Schreiben
};

struct AudioStatus {
    bool running{false};
    std::string source;      // z. B. "Gerät: Mikrofon" oder "Datei: beat.wav"; leer, wenn gestoppt
    std::uint64_t frames{0};
};

class AudioStore {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    /// `now` ersetzt die Uhr (für Tests); Standard ist steady_clock.
    explicit AudioStore(Clock now = [] { return std::chrono::steady_clock::now(); }) : now_{std::move(now)} {}

    void write(const AudioFrame& frame);
    /// Der letzte Frame, oder nichts, solange noch keiner geschrieben wurde.
    [[nodiscard]] std::optional<AudioSnapshot> read() const;

    void setSource(bool running, std::string description);
    [[nodiscard]] AudioStatus status() const;

    /// Vergisst den letzten Frame (beim Stoppen der Quelle).
    void clear();

private:
    Clock now_;
    mutable std::mutex mutex_;
    std::optional<AudioFrame> frame_;
    std::chrono::steady_clock::time_point updated_{};
    AudioStatus status_;
};

}  // namespace whitehole::audio
