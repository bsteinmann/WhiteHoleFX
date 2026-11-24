// session.cpp: Zusammenbau und Betrieb eines Projekts.
#include "whitehole/runtime/session.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#include "whitehole/core/hash.hpp"
#include "whitehole/engine/readback_ring.hpp"
#include "whitehole/executor/sampling_shader.hpp"

namespace whitehole::runtime {

namespace {

/// Die sACN-Sender-ID muss zwischen Programmläufen gleich bleiben, sonst melden Empfänger
/// bei jedem Start eine neue Quelle. Sie wird darum aus dem Quellnamen abgeleitet.
std::array<std::uint8_t, 16> senderId(const std::string& sourceName) {
    std::array<std::uint8_t, 16> cid{};
    const std::uint64_t a = fnv1a64("whitehole:cid:a:" + sourceName);
    const std::uint64_t b = fnv1a64("whitehole:cid:b:" + sourceName);
    for (std::size_t i = 0; i < 8; ++i) {
        cid[i] = static_cast<std::uint8_t>(a >> (8 * i));
        cid[8 + i] = static_cast<std::uint8_t>(b >> (8 * i));
    }
    return cid;
}

}  // namespace

/// Reicht Frames an den Ausgabe-Thread weiter. Zwei Aufgaben davor: ein aktives Testbild ersetzt
/// die Farben des Effekts, und auf Wunsch merkt sich die Klasse die Farben des letzten Frames.
class Session::ColorTap final : public engine::IFrameSink {
public:
    ColorTap(engine::IFrameSink& next, mapping::LedMap map, bool keepLatest)
        : next_{next}, map_{std::move(map)}, ledCount_{map_.size()}, keepLatest_{keepLatest} {}

    void publish(engine::PixelFrame frame) override {
        if (frame.bytes.size() >= ledCount_ * sizeof(ColorRgba)) {
            TestPattern pattern;
            std::chrono::steady_clock::time_point since;
            {
                const std::lock_guard<std::mutex> lock{mutex_};
                pattern = pattern_;
                since = since_;
            }
            const float level = blackout_.load() ? 0.0F : master_.load();
            std::vector<ColorRgba> colors;
            bool changed = false;
            const auto load = [&]() {
                if (colors.empty()) {
                    colors.resize(ledCount_);
                    std::memcpy(colors.data(), frame.bytes.data(), ledCount_ * sizeof(ColorRgba));
                }
            };
            if (pattern.active()) {
                const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
                colors = renderTestPattern(pattern, map_, seconds);
                changed = true;
            }
            if (level != 1.0F) {
                load();
                for (ColorRgba& c : colors) {
                    c.r *= level;
                    c.g *= level;
                    c.b *= level;
                }
                changed = true;
            }
            if (changed) {
                std::memcpy(frame.bytes.data(), colors.data(), ledCount_ * sizeof(ColorRgba));
            }
            if (keepLatest_) {
                load();
                const std::lock_guard<std::mutex> lock{mutex_};
                latest_ = std::move(colors);
            }
            takeSecondary(frame.bytes);
        }
        {
            // DMX-Kanäle der Output-Nodes: der Stand der letzten Auswertung (höchstens einen Frame alt, weil das Bild
            // erst nach dem Readback hier ankommt).
            const std::lock_guard<std::mutex> lock{mutex_};
            frame.channels = channels_;
        }
        next_.publish(std::move(frame));
    }

    /// Zusatzbereich hinter den Hauptfarben (siehe sampling_shader.hpp): Kopf mit Kennung, dann eine Farbe je LED.
    void takeSecondary(const std::vector<std::byte>& bytes) {
        const std::uint32_t expected = expectedTag_.load();
        if (expected == 0) {
            return;
        }
        const std::size_t head = ledCount_ * sizeof(ColorRgba);
        if (bytes.size() < head + (ledCount_ + 1) * sizeof(ColorRgba)) {
            return;
        }
        float tag = 0.0F;
        std::memcpy(&tag, bytes.data() + head, sizeof tag);
        if (static_cast<std::uint32_t>(tag) != expected) {
            return;  // ein Readback von vor der Wahl, oder der Bereich wurde nicht neu geschrieben
        }
        std::vector<ColorRgba> colors(ledCount_);
        std::memcpy(colors.data(), bytes.data() + head + sizeof(ColorRgba), ledCount_ * sizeof(ColorRgba));
        const std::lock_guard<std::mutex> lock{mutex_};
        secondary_ = std::move(colors);
    }

    void expectSecondary(std::uint32_t tag) {
        expectedTag_.store(tag);
        const std::lock_guard<std::mutex> lock{mutex_};
        secondary_.clear();
    }

    std::vector<ColorRgba> secondary() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return secondary_;
    }

    void setChannels(std::vector<ChannelWrite> channels) {
        const std::lock_guard<std::mutex> lock{mutex_};
        channels_ = std::move(channels);
    }

    std::vector<ColorRgba> latest() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return latest_;
    }

    void setLevel(float master, bool blackout) {
        master_.store(master);
        blackout_.store(blackout);
    }

    void setPattern(TestPattern pattern) {
        const std::lock_guard<std::mutex> lock{mutex_};
        pattern_ = std::move(pattern);
        since_ = std::chrono::steady_clock::now();
    }

private:
    engine::IFrameSink& next_;
    mapping::LedMap map_;
    std::size_t ledCount_;
    bool keepLatest_;
    std::atomic<float> master_{1.0F};
    std::atomic<bool> blackout_{false};
    mutable std::mutex mutex_;
    std::vector<ColorRgba> latest_;
    std::vector<ColorRgba> secondary_;
    std::atomic<std::uint32_t> expectedTag_{0};  // 0 = kein Zusatzausgang gewählt
    std::vector<ChannelWrite> channels_;
    TestPattern pattern_;
    std::chrono::steady_clock::time_point since_{std::chrono::steady_clock::now()};
};

Result<OutputParts> buildOutput(const project::Project& project, output::IPacketSink& sink,
                                output::SentDmxStore* sentDmx) {
    if (project.layout.fixtures().empty()) {
        return Error{ErrorCode::InvalidArgument, "Das Projekt enthält kein Fixture"};
    }
    if (project.patches.empty()) {
        return Error{ErrorCode::InvalidArgument, "Kein Fixture ist gepatcht; es gäbe keine Ausgabe"};
    }
    auto map = project.layout.bake(project.normalize);
    if (!map.ok()) {
        return map.error();
    }
    auto plan = output::OutputPlan::create(map.value(), project.patches);
    if (!plan.ok()) {
        return plan.error();
    }
    output::OutputConfig config;
    config.destinations = project.destinations;
    config.dmxPatches = project.dmxPatches;
    config.cid = senderId(project.sourceName);
    config.sourceName = project.sourceName;
    config.sacnPriority = project.sacnPriority;
    config.sentDmx = sentDmx;
    auto pipeline = output::OutputPipeline::create(std::move(plan).value(), std::move(config), sink);
    if (!pipeline.ok()) {
        return pipeline.error();
    }
    return OutputParts{std::move(map).value(), std::move(pipeline).value()};
}

Result<std::unique_ptr<Session>> Session::create(const project::Project& project, SessionDeps deps,
                                                 SessionOptions options) {
    if (!project.target) {
        return Error{ErrorCode::InvalidArgument, "Das Projekt hat keine \"output\"-Zeile (welcher Ausgang soll leuchten?)"};
    }
    auto parts = buildOutput(project, deps.sink, deps.sentDmx);
    if (!parts.ok()) {
        return parts.error();
    }
    const mapping::LedMap map = parts.value().map;

    executor::SamplingStageConfig stageConfig;
    stageConfig.target = *project.target;
    stageConfig.ledCapacity = static_cast<std::uint32_t>(map.world.size());
    stageConfig.secondary = options.secondaryColors;
    auto created = executor::SamplingRenderStage::create(deps.device, deps.snippets, deps.kernels, stageConfig);
    if (!created.ok()) {
        return created.error();
    }
    std::shared_ptr<executor::SamplingRenderStage> stage{std::move(created).value()};
    if (const Result<void> r = stage->setMap(map); !r.ok()) {
        return r.error();
    }
    // Ein Graph, der sich nicht übersetzen lässt, soll beim Start auffallen und nicht erst als
    // schwarzes Bild: setGraph meldet den Fehler und merkt ihn sich zusätzlich in der Stage.
    if (const Result<void> r = stage->setGraph(project.graph, options.secondaryTargets); !r.ok()) {
        return r.error();
    }

    auto worker = std::make_unique<output::OutputWorker>(std::move(parts.value().pipeline));
    auto tap = std::make_unique<ColorTap>(worker->sink(), map, options.keepLatestColors);
    auto ring = engine::ReadbackRing::create(deps.device, *tap, options.readbackSlots, stage->outputSizeBytes());
    if (!ring.ok()) {
        return ring.error();
    }
    auto engine = std::make_unique<engine::RenderEngine>(
        deps.device, deps.clock, *ring.value(),
        engine::RenderEngineConfig{stage->output(), stage->outputSizeBytes()});
    engine->setStages({stage});
    engine->setCopySizeBytes(stage->mainSizeBytes());  // den Zusatzbereich erst lesen, wenn einer gewählt ist

    return std::unique_ptr<Session>{new Session{deps.device, map, std::move(worker), std::move(tap), std::move(stage),
                                                std::move(ring).value(), std::move(engine), options.readbackSlots}};
}

Session::Session(gpu::IGpuDevice& device, mapping::LedMap map, std::unique_ptr<output::OutputWorker> worker,
                 std::unique_ptr<ColorTap> tap, std::shared_ptr<executor::SamplingRenderStage> stage,
                 std::unique_ptr<engine::ReadbackRing> ring, std::unique_ptr<engine::RenderEngine> engine,
                 std::size_t slots)
    : device_{device},
      map_{std::move(map)},
      worker_{std::move(worker)},
      tap_{std::move(tap)},
      stage_{std::move(stage)},
      ring_{std::move(ring)},
      engine_{std::move(engine)},
      slots_{slots} {}

Session::~Session() {
    // Laufende Readbacks zuerst abwarten, damit ihre Rückrufe nicht ins Leere zeigen.
    (void)flush(std::chrono::milliseconds{500});
}

bool Session::flush(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
        device_.poll();
        const SessionStats s = stats();
        const std::uint64_t delivered = s.engine.framesRendered - std::min(s.engine.framesRendered, s.failedReadbacks);
        const std::uint64_t handled = s.output.framesProcessed + s.frameErrors + s.mailboxDrops;
        if (ring_->freeSlots() == slots_ && handled >= delivered) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
}

SessionStats Session::stats() const {
    SessionStats s;
    s.engine = engine_->stats();
    s.output = worker_->pipeline().stats();
    s.mailboxDrops = worker_->droppedFrames();
    s.frameErrors = worker_->frameErrors();
    s.failedReadbacks = ring_->failedReadbacks();
    return s;
}

std::vector<ColorRgba> Session::latestColors() const { return tap_->latest(); }

std::vector<ColorRgba> Session::latestSecondaryColors() const { return tap_->secondary(); }

Result<void> Session::selectSecondary(std::optional<std::uint32_t> extra) {
    if (!stage_->hasSecondary()) {
        return {};
    }
    // Die Kennung steht im Shader als f32: unter 2^24 bleibt sie exakt; 0 heißt "keiner".
    secondaryTag_ = secondaryTag_ % 0xFFFFFFU + 1U;
    const std::uint32_t tag = extra ? secondaryTag_ : 0U;
    tap_->expectSecondary(tag);
    engine_->setCopySizeBytes(extra ? stage_->outputSizeBytes() : stage_->mainSizeBytes());
    return stage_->selectExtra(extra.value_or(executor::kNoSamplingExtra), tag);
}

Result<void> Session::renderFrame() {
    const Result<void> r = engine_->renderFrame();
    // Die CPU-Nodes sind jetzt ausgewertet: DMX-Kanäle der Output-Nodes für den Ausgabe-Thread bereitlegen.
    const values::ValueStore::ChannelTable& table = stage_->values().allChannels();
    if (!table.empty() || hadChannels_) {
        std::vector<ChannelWrite> channels;
        channels.reserve(table.size());
        for (const auto& [node, write] : table) {
            channels.push_back(write);
        }
        tap_->setChannels(std::move(channels));
        hadChannels_ = !table.empty();
    }
    return r;
}

void Session::setTestPattern(TestPattern pattern) { tap_->setPattern(std::move(pattern)); }

void Session::setOutputLevel(float master, bool blackout) { tap_->setLevel(master, blackout); }

std::optional<Error> Session::lastOutputError() const {
    if (const auto frameError = worker_->lastFrameError()) {
        return frameError;
    }
    return worker_->pipeline().lastSendError();
}

std::optional<Error> Session::stageError() const {
    if (stage_->graphError()) {
        return stage_->graphError();
    }
    return stage_->frameError();
}

}  // namespace whitehole::runtime
