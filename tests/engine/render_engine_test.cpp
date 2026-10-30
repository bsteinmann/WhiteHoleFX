#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "fake_gpu_device.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/engine/latest_frame_mailbox.hpp"
#include "whitehole/engine/readback_ring.hpp"
#include "whitehole/engine/render_engine.hpp"

using namespace whitehole;
using namespace whitehole::engine;
using whitehole::test::FakeGpuDevice;

namespace {

class RecordingStage final : public IRenderStage {
public:
    void record(const FrameTime& time, gpu::ICommandRecorder& /*recorder*/) override {
        frames.push_back(time.frame);
        times.push_back(time.time.count());
    }

    std::vector<FrameIndex> frames;
    std::vector<double> times;
};

/// Komplett verdrahtete Engine auf dem FakeGpuDevice.
struct Rig {
    static constexpr std::uint64_t kBytes = 8;

    explicit Rig(std::size_t slots) {
        auto created = ReadbackRing::create(device, mailbox, slots, kBytes);
        ring = std::move(created).value();

        const auto buffer = device.createBuffer(gpu::BufferDesc{
            .sizeBytes = kBytes,
            .usage = gpu::Usage::Storage | gpu::Usage::CopySource,
            .label = "output"});
        output = buffer.value();

        engine = std::make_unique<RenderEngine>(device, clock, *ring,
                                                RenderEngineConfig{output, kBytes});
        engine->setStages({stage});
    }

    // Reihenfolge wichtig: Der Ring muss vor dem Gerät zerstört werden.
    FakeGpuDevice device;
    LatestFrameMailbox mailbox;
    ManualFrameClock clock{Seconds{0.5}};
    std::unique_ptr<ReadbackRing> ring;
    gpu::BufferHandle output;
    std::shared_ptr<RecordingStage> stage = std::make_shared<RecordingStage>();
    std::unique_ptr<RenderEngine> engine;
};

}  // namespace

TEST_CASE("RenderEngine liefert die Ausgabe der Stages an die Sink") {
    Rig rig{3};
    rig.device.fill(rig.output, std::byte{0x5A});

    REQUIRE(rig.engine->renderFrame().ok());
    rig.device.poll();  // schließt das Readback ab

    const auto frame = rig.mailbox.takeLatest();
    REQUIRE(frame.has_value());
    CHECK(frame->frame == 1U);
    REQUIRE(frame->bytes.size() == Rig::kBytes);
    CHECK(std::to_integer<unsigned>(frame->bytes[0]) == 0x5AU);
    CHECK(rig.stage->frames.size() == 1U);
    CHECK(rig.engine->stats().framesRendered == 1U);
    CHECK(rig.engine->stats().framesDropped == 0U);
}

TEST_CASE("RenderEngine gibt den Stages Frame-Index und Zeit der Uhr") {
    Rig rig{3};
    for (int i = 0; i < 3; ++i) {
        REQUIRE(rig.engine->renderFrame().ok());
        rig.device.poll();
    }

    const std::vector<FrameIndex> expectedFrames{1, 2, 3};
    const std::vector<double> expectedTimes{0.5, 1.0, 1.5};
    CHECK(rig.stage->frames == expectedFrames);
    CHECK(rig.stage->times == expectedTimes);
}

TEST_CASE("RenderEngine verwirft Frames ohne freien Slot, die Uhr läuft weiter") {
    Rig rig{2};
    rig.device.holdReadbacks(true);

    CHECK(rig.engine->renderFrame().ok());
    CHECK(rig.engine->renderFrame().ok());
    CHECK(rig.engine->renderFrame().ok());  // kein Slot frei: verworfen, kein Fehler

    CHECK(rig.engine->stats().framesRendered == 2U);
    CHECK(rig.engine->stats().framesDropped == 1U);
    CHECK(rig.stage->frames.size() == 2U);  // die Stages liefen für Frame 3 nicht

    rig.device.holdReadbacks(false);
    CHECK(rig.engine->renderFrame().ok());  // poll() am Anfang gibt beide Slots frei

    // Frame 4 hat die Uhr-Nummer 4: Frame 3 wurde übersprungen, nicht wiederholt.
    const std::vector<FrameIndex> expectedFrames{1, 2, 4};
    CHECK(rig.stage->frames == expectedFrames);

    // Beim poll() wurden Frame 1 und 2 geliefert; Frame 1 wurde überschrieben.
    CHECK(rig.mailbox.droppedFrames() == 1U);
    const auto older = rig.mailbox.takeLatest();
    REQUIRE(older.has_value());
    CHECK(older->frame == 2U);

    rig.device.poll();
    const auto newest = rig.mailbox.takeLatest();
    REQUIRE(newest.has_value());
    CHECK(newest->frame == 4U);
}

TEST_CASE("RenderEngine gibt den Slot frei, wenn das Einreichen scheitert") {
    Rig rig{1};
    rig.device.failNextSubmit();

    const Result<void> failed = rig.engine->renderFrame();
    REQUIRE_FALSE(failed.ok());
    CHECK(failed.error().code == ErrorCode::GpuError);
    CHECK(rig.ring->freeSlots() == 1U);
    CHECK(rig.engine->stats().framesRendered == 0U);

    CHECK(rig.engine->renderFrame().ok());  // der nächste Frame läuft normal
    CHECK(rig.engine->stats().framesRendered == 1U);
}
