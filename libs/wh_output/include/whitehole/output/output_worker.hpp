// output_worker.hpp: der Ausgabe-Thread, der Frames aus der Engine entgegennimmt.
//
// Warum es diese Datei gibt: Die Engine ruft IFrameSink::publish() auf ihrem eigenen
// Thread auf und darf dort nicht warten. Farben umrechnen und Pakete senden dauert aber
// (bei 100.000 LEDs mehrere Millisekunden). Der Worker entkoppelt beides: publish()
// legt den Frame nur in einen Briefkasten (LatestFrameMailbox), ein eigener Thread holt
// ihn und lässt die OutputPipeline laufen. Ist der Ausgang langsamer als die Engine,
// überspringt er alte Frames, statt sie aufzustauen: Latenz geht vor Vollständigkeit.
//
// Aufgabe: Thread starten und beenden. Der Frame-Eingang ist `sink()`, der bei der
// RenderEngine als IFrameSink eingehängt wird.
//
// Fehler eines einzelnen Frames (z. B. zu kleiner Puffer) werden gezählt und in
// `lastFrameError()` gemerkt; der Thread läuft weiter.
//
// Abhängigkeiten: wh_engine (Briefkasten, PixelFrame), OutputPipeline.
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "whitehole/engine/latest_frame_mailbox.hpp"
#include "whitehole/output/output_pipeline.hpp"

namespace whitehole::output {

class OutputWorker {
public:
    /// Startet den Thread sofort.
    explicit OutputWorker(std::unique_ptr<OutputPipeline> pipeline);

    /// Beendet den Thread und wartet auf ihn. Ein gerade laufender Frame wird fertig.
    ~OutputWorker();
    OutputWorker(const OutputWorker&) = delete;
    OutputWorker& operator=(const OutputWorker&) = delete;

    /// Hier gibt die Engine ihre Frames ab (thread-sicher, blockiert nie).
    [[nodiscard]] engine::IFrameSink& sink() noexcept { return mailbox_; }

    [[nodiscard]] const OutputPipeline& pipeline() const noexcept { return *pipeline_; }
    [[nodiscard]] std::uint64_t droppedFrames() const { return mailbox_.droppedFrames(); }
    [[nodiscard]] std::uint64_t frameErrors() const noexcept { return frameErrors_.load(); }
    [[nodiscard]] std::optional<Error> lastFrameError() const;

private:
    void run(const std::atomic<bool>& stop);

    std::unique_ptr<OutputPipeline> pipeline_;
    engine::LatestFrameMailbox mailbox_;
    std::atomic<std::uint64_t> frameErrors_{0};
    mutable std::mutex errorMutex_;
    std::optional<Error> lastError_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

}  // namespace whitehole::output
