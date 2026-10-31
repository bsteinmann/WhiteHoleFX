// session.cpp: Zusammenbau und Betrieb eines Projekts.
#include "whitehole/runtime/session.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <thread>

#include "whitehole/core/hash.hpp"
#include "whitehole/engine/readback_ring.hpp"

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

/// Reicht Frames an den Ausgabe-Thread weiter und behält die Farben des letzten.
class Session::ColorTap final : public engine::IFrameSink {
public:
    ColorTap(engine::IFrameSink& next, std::size_t ledCount) : next_{next}, ledCount_{ledCount} {}

    void publish(engine::PixelFrame frame) override {
        if (frame.bytes.size() >= ledCount_ * sizeof(ColorRgba)) {
            std::vector<ColorRgba> colors(ledCount_);
            std::memcpy(colors.data(), frame.bytes.data(), ledCount_ * sizeof(ColorRgba));
            const std::lock_guard<std::mutex> lock{mutex_};
            latest_ = std::move(colors);
        }
        next_.publish(std::move(frame));
    }

    std::vector<ColorRgba> latest() const {
        const std::lock_guard<std::mutex> lock{mutex_};
        return latest_;
    }

private:
    engine::IFrameSink& next_;
    std::size_t ledCount_;
    mutable std::mutex mutex_;
    std::vector<ColorRgba> latest_;
};

Result<OutputParts> buildOutput(const project::Project& project, output::IPacketSink& sink) {
    if (project.layout.fixtures().empty()) {
        return Error{ErrorCode::InvalidArgument, "Das Projekt enthält kein Fixture"};
    }
    if (project.patches.empty()) {
        return Error{ErrorCode::InvalidArgument, "Kein Fixture ist gepatcht; es gäbe keine Ausgabe"};
    }
    auto map = project.layout.bake(project.normalize, project.region);
    if (!map.ok()) {
        return map.error();
    }
    auto plan = output::OutputPlan::create(map.value(), project.patches);
    if (!plan.ok()) {
        return plan.error();
    }
    output::OutputConfig config;
    config.destinations = project.destinations;
    config.cid = senderId(project.sourceName);
    config.sourceName = project.sourceName;
    config.sacnPriority = project.sacnPriority;
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
    auto parts = buildOutput(project, deps.sink);
    if (!parts.ok()) {
        return parts.error();
    }
    const mapping::LedMap map = parts.value().map;

    executor::SamplingStageConfig stageConfig;
    stageConfig.target = *project.target;
    stageConfig.ledCapacity = static_cast<std::uint32_t>(map.world.size());
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
    if (const Result<void> r = stage->setGraph(project.graph); !r.ok()) {
        return r.error();
    }

    auto worker = std::make_unique<output::OutputWorker>(std::move(parts.value().pipeline));
    std::unique_ptr<ColorTap> tap;
    engine::IFrameSink* sink = &worker->sink();
    if (options.keepLatestColors) {
        tap = std::make_unique<ColorTap>(worker->sink(), map.world.size());
        sink = tap.get();
    }
    auto ring = engine::ReadbackRing::create(deps.device, *sink, options.readbackSlots, stage->outputSizeBytes());
    if (!ring.ok()) {
        return ring.error();
    }
    auto engine = std::make_unique<engine::RenderEngine>(
        deps.device, deps.clock, *ring.value(),
        engine::RenderEngineConfig{stage->output(), stage->outputSizeBytes()});
    engine->setStages({stage});

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

std::vector<ColorRgba> Session::latestColors() const { return tap_ ? tap_->latest() : std::vector<ColorRgba>{}; }

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
