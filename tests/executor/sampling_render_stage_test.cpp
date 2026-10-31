// SamplingRenderStage: Puffer, Karte, Dispatch und die Anbindung an die Engine.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "stage_fixture.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/engine/latest_frame_mailbox.hpp"
#include "whitehole/engine/readback_ring.hpp"
#include "whitehole/engine/render_engine.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/executor/sampling_shader.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::test;

namespace {

mapping::LedMap bake(std::uint32_t cols, std::uint32_t rows) {
    mapping::MatrixShape matrix;
    matrix.columns = cols;
    matrix.rows = rows;
    mapping::Layout layout;
    REQUIRE(layout.add("Panel", Vec3{}, matrix).ok());
    return layout.bake().value();
}

/// Gradient als Ziel, Sampling-Stage mit Platz für `capacity` LEDs.
struct SamplingFixture {
    Lab lab;
    FakeGpuDevice device;
    NodeId gradient;
    std::unique_ptr<executor::SamplingRenderStage> stage;
    std::uint64_t frame{0};

    explicit SamplingFixture(std::uint32_t capacity = 64) {
        gradient = lab.add("wh.generator.gradient");
        executor::SamplingStageConfig config;
        config.target = graph::PortRef{gradient, "out"};
        config.ledCapacity = capacity;
        auto created = executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE(created.ok());
        stage = std::move(created.value());
    }

    void render(double seconds = 0.0) {
        FrameTime time;
        time.frame = frame++;
        time.time = Seconds{seconds};
        REQUIRE(device.submit([&](gpu::ICommandRecorder& r) { stage->record(time, r); }).ok());
    }
    const FakeGpuDevice::DispatchRecord& last() const {
        REQUIRE_FALSE(device.dispatches().empty());
        return device.dispatches().back();
    }
};

}  // namespace

TEST_CASE("create legt Positions-, Farb-, Info- und Frame-Puffer an") {
    SamplingFixture fx{100};
    CHECK(fx.device.liveBufferCount() == 4U);
    CHECK(fx.device.liveTextureCount() == 0U);
    CHECK(fx.device.livePipelineCount() == 0U);
    CHECK(fx.stage->outputSizeBytes() == 1600U);
    CHECK(fx.stage->ledCount() == 0U);
    CHECK(fx.device.bufferBytes(fx.stage->output()).size() == 1600U);
}

TEST_CASE("create lehnt ungültige Kapazitäten ab und räumt bei Fehlern auf") {
    FakeGpuDevice device;
    Lab lab;
    executor::SamplingStageConfig config;
    config.target = graph::PortRef{NodeId{1}, "out"};

    config.ledCapacity = 0;
    REQUIRE_FALSE(executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config).ok());
    config.ledCapacity = (1U << 23) + 1U;
    REQUIRE_FALSE(executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config).ok());
    CHECK(device.liveBufferCount() == 0U);

    config.ledCapacity = 8;
    for (int failAt = 0; failAt < 4; ++failAt) {  // Positionen, Farben, Info, Frame-Puffer
        CAPTURE(failAt);
        device.failBufferAfter(failAt);
        const auto created = executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE_FALSE(created.ok());
        CHECK(created.error().code == ErrorCode::GpuError);
        CHECK(device.liveBufferCount() == 0U);  // nichts bleibt liegen
    }
}

TEST_CASE("setMap lädt Positionen und Anzahl hoch") {
    SamplingFixture fx{64};
    const mapping::LedMap map = bake(4, 2);
    REQUIRE(fx.stage->setMap(map).ok());
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK(fx.stage->ledCount() == 8U);

    fx.render();
    const auto positions = fx.device.bufferBytes(bufferAt(fx.last(), 1, 0));
    const auto packed = executor::packPositions(map.unit);
    REQUIRE(positions.size() >= packed.size());
    CHECK(std::vector<std::byte>(positions.begin(), positions.begin() + static_cast<std::ptrdiff_t>(packed.size())) ==
          packed);

    const auto info = fx.device.bufferBytes(bufferAt(fx.last(), 1, 2));
    REQUIRE(info.size() == 16U);
    std::uint32_t count = 0;
    std::memcpy(&count, info.data(), sizeof count);
    CHECK(count == 8U);
}

TEST_CASE("setMap lehnt leere und zu große Karten ab und behält die alte") {
    SamplingFixture fx{16};
    REQUIRE(fx.stage->setMap(bake(4, 2)).ok());

    const Result<void> empty = fx.stage->setMap(mapping::LedMap{});
    REQUIRE_FALSE(empty.ok());
    CHECK(empty.error().code == ErrorCode::InvalidArgument);

    const Result<void> tooBig = fx.stage->setMap(bake(5, 4));  // 20 > 16
    REQUIRE_FALSE(tooBig.ok());
    CHECK(tooBig.error().code == ErrorCode::InvalidArgument);
    // Abgelehnt wegen der Kapazität, nicht erst vom Gerät beim Hochladen.
    CHECK(tooBig.error().message.find("Kapazität") != std::string::npos);

    CHECK(fx.stage->ledCount() == 8U);
    CHECK(fx.stage->setMap(bake(4, 4)).ok());  // genau die Kapazität
    CHECK(fx.stage->ledCount() == 16U);
}

TEST_CASE("Scheitert der Upload der Karte, bleibt die alte aktiv") {
    SamplingFixture fx{64};
    REQUIRE(fx.stage->setMap(bake(4, 2)).ok());

    fx.device.failWriteAfter(0);  // Positionen
    REQUIRE_FALSE(fx.stage->setMap(bake(8, 4)).ok());
    CHECK(fx.stage->ledCount() == 8U);

    fx.device.failWriteAfter(1);  // Positionen gehen durch, die Anzahl nicht
    REQUIRE_FALSE(fx.stage->setMap(bake(8, 4)).ok());
    CHECK(fx.stage->ledCount() == 8U);
}

TEST_CASE("Ohne Karte oder ohne Graph wird nichts aufgezeichnet") {
    SamplingFixture fx{64};
    fx.render();  // weder Karte noch Graph
    CHECK(fx.device.dispatches().empty());

    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render();  // Graph, aber keine Karte
    CHECK(fx.device.dispatches().empty());
    CHECK(fx.stage->stats().framesSkipped == 2U);

    REQUIRE(fx.stage->setMap(bake(4, 2)).ok());
    fx.render();
    CHECK(fx.device.dispatches().size() == 1U);
    CHECK(fx.stage->stats().framesRecorded == 1U);
}

TEST_CASE("Karte ohne Graph: der Frame entfällt und wird gezählt") {
    SamplingFixture fx{64};
    REQUIRE(fx.stage->setMap(bake(4, 2)).ok());
    fx.render();
    CHECK(fx.device.dispatches().empty());
    CHECK(fx.stage->stats().framesSkipped == 1U);
}

TEST_CASE("Der Dispatch bindet Feldbibliothek (Gruppe 0) und LED-Puffer (Gruppe 1)") {
    SamplingFixture fx{512};
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    SECTION("8 LEDs: eine Gruppe") {
        REQUIRE(fx.stage->setMap(bake(4, 2)).ok());
        fx.render();
        CHECK(fx.last().groupsX == 1U);
        CHECK(fx.last().groupsY == 1U);
    }
    SECTION("200 LEDs: vier Gruppen") {
        REQUIRE(fx.stage->setMap(bake(20, 10)).ok());
        fx.render();
        CHECK(fx.last().groupsX == 4U);
        CHECK(fx.last().groupsY == 1U);
    }
    CHECK(fx.last().groupsZ == 1U);
    REQUIRE(fx.last().bindings.size() == 5U);
    CHECK(bufferAt(fx.last(), 0, 0).value() != 0U);
    CHECK(bufferAt(fx.last(), 0, 1).value() != 0U);
    CHECK(bufferAt(fx.last(), 1, 0).value() != 0U);
    CHECK(bufferAt(fx.last(), 1, 1) == fx.stage->output());
    CHECK(bufferAt(fx.last(), 1, 2).value() != 0U);
}

TEST_CASE("Das Ausgabe-Handle bleibt bei jeder Karte gleich") {
    SamplingFixture fx{64};
    const gpu::BufferHandle output = fx.stage->output();
    REQUIRE(fx.stage->setMap(bake(8, 8)).ok());
    CHECK(fx.stage->output() == output);
    REQUIRE(fx.stage->setMap(bake(2, 2)).ok());
    CHECK(fx.stage->output() == output);
    CHECK(fx.stage->ledCount() == 4U);
    CHECK(fx.stage->outputSizeBytes() == 64U * 16U);  // Kapazität, nicht Anzahl
}

TEST_CASE("Die Pipeline enthält Feldbibliothek und Abtast-Einstieg") {
    SamplingFixture fx{8};
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    REQUIRE(fx.device.pipelineDescs().size() == 1U);
    const std::string& code = fx.device.pipelineDescs().back().shader.code;
    CHECK(code.find("fn wh_field(") != std::string::npos);
    CHECK(code.find("wh_positions") != std::string::npos);
}

TEST_CASE("Durchstich: Time steuert Rotate.angle auch im Abtast-Pass") {
    Lab lab;
    FakeGpuDevice device;
    const NodeId gradient = lab.add("wh.generator.gradient");
    const NodeId rotate = lab.add("wh.transform.rotate");
    const NodeId time = lab.add("wh.value.time");
    lab.connect(gradient, "out", rotate, "source");
    lab.connect(time, "seconds", rotate, "angle");

    executor::SamplingStageConfig config;
    config.target = graph::PortRef{rotate, "out"};
    config.ledCapacity = 8;
    auto stage = executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config).value();
    REQUIRE(stage->setMap(bake(4, 2)).ok());
    REQUIRE(stage->setGraph(lab.graph).ok());

    const auto compiled = compiler::compileField(lab.graph, lab.snippets, config.target);
    const compiler::ParameterSlot* angle = slotFor(compiled.value().layout, rotate, "angle");
    REQUIRE(angle != nullptr);

    FrameTime t;
    t.time = Seconds{12.0};
    REQUIRE(device.submit([&](gpu::ICommandRecorder& r) { stage->record(t, r); }).ok());
    const auto params = device.bufferBytes(bufferAt(device.dispatches().back(), 0, 1));
    CHECK(floatAt(params, angle->offsetBytes) == 12.0F);
}

TEST_CASE("Die Stage gibt alle Puffer und die Pipeline frei") {
    SamplingFixture fx{64};
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK(fx.device.liveBufferCount() == 5U);  // 3 eigene + Frame + Parameter
    CHECK(fx.device.livePipelineCount() == 1U);
    fx.stage.reset();
    CHECK(fx.device.liveBufferCount() == 0U);
    CHECK(fx.device.livePipelineCount() == 0U);
}

TEST_CASE("Die Engine kopiert den Farbpuffer der Stage in den Readback-Ring") {
    FakeGpuDevice device;
    Lab lab;
    const NodeId gradient = lab.add("wh.generator.gradient");
    executor::SamplingStageConfig config;
    config.target = graph::PortRef{gradient, "out"};
    config.ledCapacity = 8;
    std::shared_ptr<executor::SamplingRenderStage> stage =
        executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config).value();
    REQUIRE(stage->setMap(bake(4, 2)).ok());
    REQUIRE(stage->setGraph(lab.graph).ok());

    engine::LatestFrameMailbox mailbox;
    engine::ManualFrameClock clock{Seconds{0.5}};
    auto ring = engine::ReadbackRing::create(device, mailbox, 2, stage->outputSizeBytes()).value();
    engine::RenderEngine engine{device, clock, *ring,
                                engine::RenderEngineConfig{stage->output(), stage->outputSizeBytes()}};
    engine.setStages({stage});

    REQUIRE(engine.renderFrame().ok());
    CHECK(engine.stats().framesRendered == 1U);
    CHECK(device.dispatches().size() == 1U);
    CHECK(stage->stats().framesRecorded == 1U);
}

TEST_CASE("setParameters übernimmt nur Werte: keine neue Pipeline, neue Zahlen im Puffer") {
    SamplingFixture fx;
    REQUIRE(fx.stage->setMap(bake(4, 4)).ok());
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    const gpu::ComputePipelineHandle first = fx.last().pipeline;
    REQUIRE(fx.stage->stats().pipelinesBuilt == 1U);

    const auto before = fx.device.bufferBytes(bufferAt(fx.last(), 0, 1));
    fx.lab.set(fx.gradient, "angle", 90.0);
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());
    fx.render(1.0);
    CHECK(fx.stage->stats().pipelinesBuilt == 1U);  // nichts neu übersetzt
    CHECK(fx.last().pipeline == first);
    CHECK(fx.device.bufferBytes(bufferAt(fx.last(), 0, 1)) != before);  // aber der neue Wert liegt im Parameterpuffer
}

TEST_CASE("setParameters ohne bisherigen Graphen verhält sich wie setGraph") {
    SamplingFixture fx;
    REQUIRE(fx.stage->setMap(bake(4, 4)).ok());
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());
    fx.render(0.0);
    CHECK(fx.stage->stats().pipelinesBuilt == 1U);
}
