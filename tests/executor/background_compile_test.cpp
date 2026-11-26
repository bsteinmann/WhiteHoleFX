// FieldPass mit Übersetzen im Hintergrund (Live-Betrieb): die bisherige Pipeline rendert weiter, bis die neue fertig ist;
// zuletzt benutzte Pipelines kommen aus dem Zwischenspeicher; Fehler lassen den letzten guten Zustand laufen.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <variant>
#include <vector>
#include <memory>

#include "builtin_lab.hpp"
#include "fake_gpu_device.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::test;

namespace {

struct Fixture {
    Lab lab;
    FakeGpuDevice device;
    NodeId gradient;
    NodeId rotate;
    NodeId mix{};
    std::unique_ptr<executor::SamplingRenderStage> stage;
    std::uint64_t frame{0};

    Fixture() {
        gradient = lab.add("wh.generator.gradient");
        rotate = lab.add("wh.transform.rotate");
        lab.connect(gradient, "out", rotate, "source");
        executor::SamplingStageConfig config;
        config.target = graph::PortRef{rotate, "out"};
        config.ledCapacity = 16;
        auto created = executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE(created.ok());
        stage = std::move(created.value());
        mapping::MatrixShape matrix;
        matrix.columns = 4;
        matrix.rows = 4;
        mapping::Layout layout;
        REQUIRE(layout.add("Panel", Vec3{}, matrix).ok());
        REQUIRE(stage->setMap(layout.bake().value()).ok());
        stage->setBackgroundCompile(true);
        device.holdCompiles(true);
    }
    /// Andere Struktur: Rotate bekommt Mix(Gradient, Plasma) statt Gradient.
    void insertMix() {
        const graph::Connection* old = lab.graph.findInputConnection(rotate, "source");
        REQUIRE(old != nullptr);
        REQUIRE(lab.graph.disconnect(old->id).ok());
        const NodeId plasma = lab.add("wh.generator.plasma");
        mix = lab.add("wh.blend.mix");
        lab.connect(gradient, "out", mix, "a");
        lab.connect(plasma, "out", mix, "b");
        lab.connect(mix, "out", rotate, "source");
    }
    /// Zurück zur ersten Struktur (Gradient direkt an Rotate).
    void removeMix() {
        const graph::Connection* old = lab.graph.findInputConnection(rotate, "source");
        REQUIRE(old != nullptr);
        REQUIRE(lab.graph.disconnect(old->id).ok());
        lab.connect(gradient, "out", rotate, "source");
    }
    gpu::ComputePipelineHandle render() {
        FrameTime time;
        time.frame = frame++;
        REQUIRE(device.submit([&](gpu::ICommandRecorder& r) { stage->record(time, r); }).ok());
        REQUIRE_FALSE(device.dispatches().empty());
        return device.dispatches().back().pipeline;
    }
};

}  // namespace

TEST_CASE("Hintergrund: die alte Pipeline rendert weiter, bis die neue fertig ist", "[executor][background]") {
    Fixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());  // erste Pipeline: sofort
    CHECK(fx.device.compilesBegun() == 0);
    const gpu::ComputePipelineHandle first = fx.render();

    fx.insertMix();
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK(fx.device.compilesBegun() == 1);
    CHECK(fx.stage->graphPending());
    CHECK(fx.render() == first);  // kein Stillstand: der bisherige Stand läuft
    CHECK(fx.render() == first);

    fx.device.finishCompiles();
    const gpu::ComputePipelineHandle second = fx.render();
    CHECK(second != first);
    CHECK_FALSE(fx.stage->graphPending());
    CHECK(fx.stage->stats().pipelinesBuilt == 2U);
    CHECK(fx.device.isPipelineAlive(first));  // im Zwischenspeicher

    // Zurück zur ersten Struktur: kommt aus dem Zwischenspeicher, sofort und ohne neues Übersetzen
    fx.removeMix();
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK_FALSE(fx.stage->graphPending());
    CHECK(fx.device.compilesBegun() == 1);
    CHECK(fx.render() == first);
    CHECK(fx.stage->stats().pipelinesBuilt == 2U);
}

TEST_CASE("Hintergrund: zurück zur laufenden Struktur verwirft die wartende", "[executor][background]") {
    Fixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    const gpu::ComputePipelineHandle first = fx.render();
    fx.insertMix();
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    REQUIRE(fx.stage->graphPending());
    fx.removeMix();
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK_FALSE(fx.stage->graphPending());
    CHECK(fx.device.compilesCancelled() == 1);
    CHECK(fx.render() == first);
}

TEST_CASE("Hintergrund: ein Treiberfehler lässt den letzten guten Zustand laufen", "[executor][background]") {
    Fixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    const gpu::ComputePipelineHandle first = fx.render();
    fx.device.failNextCompile();
    fx.insertMix();
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.device.finishCompiles();
    CHECK(fx.render() == first);
    CHECK_FALSE(fx.stage->graphPending());
    REQUIRE(fx.stage->graphError().has_value());
    CHECK(fx.stage->graphError()->code == ErrorCode::ShaderError);
}

TEST_CASE("Hintergrund: Werte während des Wartens gelten ab dem Umschalten", "[executor][background]") {
    Fixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    (void)fx.render();
    fx.insertMix();
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.lab.set(fx.mix, "amount", 0.75);
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());  // geht an die wartende Struktur
    CHECK(fx.stage->graphPending());
    fx.device.finishCompiles();
    (void)fx.render();
    const auto compiled = compiler::compileField(fx.lab.graph, fx.lab.snippets, graph::PortRef{fx.rotate, "out"});
    REQUIRE(compiled.ok());
    const compiler::ParameterSlot* amount = nullptr;
    for (const compiler::ParameterSlot& slot : compiled.value().layout.slots) {
        if (slot.node == fx.mix && slot.key == "amount") {
            amount = &slot;
        }
    }
    REQUIRE(amount != nullptr);
    const gpu::BufferHandle params = std::get<gpu::BufferHandle>(fx.device.dispatches().back().bindings[1].resource);
    const std::vector<std::byte> bytes = fx.device.bufferBytes(params);
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + amount->offsetBytes, sizeof value);
    CHECK(value == 0.75F);
}

// Vorab übersetzen (FieldPass::prefetch): Live-Betrieb, die Felder des laufenden Looks (Rückmeldung vom 26.11.).
namespace {

/// Der Graph mit Mix (zweite Struktur); danach ist das Labor wieder bei der ersten Struktur.
graph::Graph mixVariant(Fixture& fx) {
    fx.insertMix();
    graph::Graph copy = fx.lab.graph;
    fx.removeMix();
    return copy;
}

}  // namespace

TEST_CASE("Vorab: eine vorab übersetzte Struktur ist sofort da", "[executor][background][prefetch]") {
    Fixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    const gpu::ComputePipelineHandle first = fx.render();
    const graph::Graph wanted = mixVariant(fx);

    REQUIRE(fx.stage->prefetch(wanted).ok());
    CHECK(fx.device.prefetchesBegun() == 1);
    CHECK(fx.device.compilesBegun() == 0);
    CHECK(fx.stage->prefetchesOpen() == 1U);
    CHECK_FALSE(fx.stage->graphPending());
    CHECK(fx.render() == first);  // das laufende Bild bleibt, wie es ist
    fx.device.finishCompiles();
    CHECK(fx.render() == first);
    CHECK(fx.stage->prefetchesOpen() == 0U);  // fertig: liegt im Zwischenspeicher

    REQUIRE(fx.stage->setGraph(wanted).ok());
    CHECK_FALSE(fx.stage->graphPending());  // sofort, ohne zu warten
    CHECK(fx.device.compilesBegun() == 0);
    CHECK(fx.render() != first);
}

TEST_CASE("Vorab: ein noch laufender Vorab-Auftrag wird vorgezogen, nicht doppelt übersetzt", "[executor][background][prefetch]") {
    Fixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    const gpu::ComputePipelineHandle first = fx.render();
    const graph::Graph wanted = mixVariant(fx);
    REQUIRE(fx.stage->prefetch(wanted).ok());

    REQUIRE(fx.stage->setGraph(wanted).ok());  // gebraucht, bevor der Treiber fertig ist
    CHECK(fx.stage->graphPending());
    CHECK(fx.device.compilesBegun() == 0);
    CHECK(fx.device.promotions() == 1);
    CHECK(fx.stage->prefetchesOpen() == 0U);
    CHECK(fx.render() == first);
    fx.device.finishCompiles();
    CHECK(fx.render() != first);
    CHECK_FALSE(fx.stage->graphPending());
}

TEST_CASE("Vorab: Bekanntes wird nicht bestellt, Verwerfen, ohne Hintergrund nichts", "[executor][background][prefetch]") {
    Fixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    (void)fx.render();
    const graph::Graph wanted = mixVariant(fx);

    REQUIRE(fx.stage->prefetch(fx.lab.graph).ok());  // läuft schon
    REQUIRE(fx.stage->prefetch(wanted).ok());
    REQUIRE(fx.stage->prefetch(wanted).ok());  // schon bestellt
    CHECK(fx.device.prefetchesBegun() == 1);
    fx.stage->cancelPrefetches();
    CHECK(fx.device.compilesCancelled() == 1);
    CHECK(fx.stage->prefetchesOpen() == 0U);

    fx.stage->setBackgroundCompile(false);
    REQUIRE(fx.stage->prefetch(wanted).ok());
    CHECK(fx.device.prefetchesBegun() == 1);  // ohne Hintergrund wird nichts vorab übersetzt
}
