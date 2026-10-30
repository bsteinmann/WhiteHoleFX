// CanvasRenderStage: Graph-Wechsel. Wann entsteht eine neue Pipeline, und was bleibt
// bei einem Fehler erhalten?
#include <catch2/catch_test_macros.hpp>

#include "stage_fixture.hpp"

using namespace whitehole;
using namespace whitehole::test;

namespace {

/// Ersetzt die Quelle von Rotate durch Mix(Gradient, <second>). Das ändert die Struktur;
/// die Parametergröße ändert sich nur, wenn der zweite Node genug neue Parameter mitbringt
/// (Plasma passt in die Lücken des Puffers, ein zweiter Gradient mit zwei Farben nicht).
NodeId insertMix(StageFixture& fx, const char* second = "wh.generator.gradient") {
    const graph::Connection* old = fx.lab.graph.findInputConnection(fx.rotate, "source");
    REQUIRE(old != nullptr);
    REQUIRE(fx.lab.graph.disconnect(old->id).ok());

    const NodeId other = fx.lab.add(second);
    const NodeId mix = fx.lab.add("wh.blend.mix");
    fx.lab.connect(fx.gradient, "out", mix, "a");
    fx.lab.connect(other, "out", mix, "b");
    fx.lab.connect(mix, "out", fx.rotate, "source");
    return mix;
}

}  // namespace

TEST_CASE("Parameteränderungen bauen keine neue Pipeline") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    const gpu::ComputePipelineHandle first = fx.lastDispatch().pipeline;
    const gpu::BufferHandle firstParams = bufferAt(fx.lastDispatch(), 0, 1);

    fx.lab.set(fx.rotate, "angle", 45.0);
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(1.0);

    CHECK(fx.stage->stats().pipelinesBuilt == 1U);
    CHECK(fx.lastDispatch().pipeline == first);
    CHECK(bufferAt(fx.lastDispatch(), 0, 1) == firstParams);  // derselbe Puffer, neuer Inhalt
    CHECK(fx.device.livePipelineCount() == 1U);

    const auto compiled = compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target());
    REQUIRE(compiled.ok());
    const compiler::ParameterSlot* angle = slotFor(compiled.value().layout, fx.rotate, "angle");
    REQUIRE(angle != nullptr);
    CHECK(floatAt(parametersOf(fx), angle->offsetBytes) == 45.0F);
}

TEST_CASE("Die Stage arbeitet mit einem Schnappschuss des Graphen") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    const auto compiled = compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target());
    REQUIRE(compiled.ok());
    const compiler::ParameterSlot* angle = slotFor(compiled.value().layout, fx.rotate, "angle");
    REQUIRE(angle != nullptr);

    fx.lab.set(fx.rotate, "angle", 30.0);  // ohne setGraph: noch nicht sichtbar
    fx.render(0.0);
    CHECK(floatAt(parametersOf(fx), angle->offsetBytes) == 0.0F);

    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(1.0);
    CHECK(floatAt(parametersOf(fx), angle->offsetBytes) == 30.0F);
}

TEST_CASE("Eine Strukturänderung baut eine neue Pipeline und gibt die alte frei") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    const gpu::ComputePipelineHandle before = fx.lastDispatch().pipeline;
    const std::size_t sizeBefore =
        compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target()).value().layout.sizeBytes;

    insertMix(fx);
    const std::size_t sizeAfter =
        compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target()).value().layout.sizeBytes;
    REQUIRE(sizeAfter != sizeBefore);  // Voraussetzung für den Puffer-Wechsel unten

    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(1.0);

    CHECK(fx.stage->stats().pipelinesBuilt == 2U);
    CHECK(fx.lastDispatch().pipeline != before);
    CHECK_FALSE(fx.device.isPipelineAlive(before));
    CHECK(fx.device.isPipelineAlive(fx.lastDispatch().pipeline));
    CHECK(fx.device.livePipelineCount() == 1U);

    // Der Parameterpuffer hat die neue Größe, der alte ist freigegeben.
    CHECK(parametersOf(fx).size() == sizeAfter);
    CHECK(fx.device.liveBufferCount() == 2U);  // Frame-Daten + Parameter
}

TEST_CASE("Bleibt die Parametergröße gleich, behält die Stage ihren Parameterpuffer") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    const gpu::BufferHandle params = bufferAt(fx.lastDispatch(), 0, 1);
    const gpu::ComputePipelineHandle before = fx.lastDispatch().pipeline;
    const std::size_t sizeBefore =
        compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target()).value().layout.sizeBytes;

    insertMix(fx, "wh.generator.plasma");
    REQUIRE(compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target()).value().layout.sizeBytes ==
            sizeBefore);
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(1.0);

    CHECK(fx.lastDispatch().pipeline != before);
    CHECK(bufferAt(fx.lastDispatch(), 0, 1) == params);
    CHECK(fx.device.liveBufferCount() == 2U);
}

TEST_CASE("Die alte Struktur zurückzubauen erzeugt wieder eine Pipeline, kein Leck") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    const NodeId mix = insertMix(fx);
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    REQUIRE(fx.lab.graph.removeNode(mix).ok());
    fx.lab.connect(fx.gradient, "out", fx.rotate, "source");
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    CHECK(fx.stage->stats().pipelinesBuilt == 3U);
    CHECK(fx.device.livePipelineCount() == 1U);
    CHECK(fx.device.liveBufferCount() == 2U);
}

TEST_CASE("Ein Compilerfehler lässt die letzte gute Ausgabe weiterlaufen") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    const gpu::ComputePipelineHandle good = fx.lastDispatch().pipeline;

    const auto compiled = compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target());
    REQUIRE(compiled.ok());
    const compiler::ParameterSlot* angle = slotFor(compiled.value().layout, fx.rotate, "angle");
    REQUIRE(angle != nullptr);

    // Der kaputte Graph trägt auch einen geänderten Wert; der darf nicht durchsickern.
    fx.lab.set(fx.rotate, "angle", 30.0);
    // Pflicht-Eingang lösen: der Graph ist nicht mehr übersetzbar.
    const graph::Connection* link = fx.lab.graph.findInputConnection(fx.rotate, "source");
    REQUIRE(link != nullptr);
    REQUIRE(fx.lab.graph.disconnect(link->id).ok());

    const Result<void> result = fx.stage->setGraph(fx.lab.graph);
    REQUIRE_FALSE(result.ok());
    REQUIRE(fx.stage->graphError().has_value());
    CHECK(fx.stage->graphError()->code == result.error().code);
    CHECK(fx.stage->graphError()->message == result.error().message);

    fx.render(1.0);
    CHECK(fx.device.dispatches().size() == 2U);
    CHECK(fx.lastDispatch().pipeline == good);
    CHECK(fx.stage->stats().pipelinesBuilt == 1U);
    CHECK(floatAt(parametersOf(fx), angle->offsetBytes) == 0.0F);  // weiter der letzte gute Graph

    // Repariert: der Fehler verschwindet.
    fx.lab.connect(fx.gradient, "out", fx.rotate, "source");
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK_FALSE(fx.stage->graphError().has_value());
}

TEST_CASE("Ein unbekanntes Ziel scheitert, ohne etwas anzulegen") {
    StageFixture fx;
    FakeGpuDevice device;
    executor::CanvasStageConfig config;
    config.extent = Extent3{8, 8, 1};
    config.target = graph::PortRef{NodeId{999}, "out"};
    auto created = executor::CanvasRenderStage::create(device, fx.lab.snippets, fx.lab.kernels, config);
    REQUIRE(created.ok());

    const Result<void> result = created.value()->setGraph(fx.lab.graph);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::NotFound);
    CHECK(device.livePipelineCount() == 0U);
    CHECK(device.liveBufferCount() == 1U);  // nur der Frame-Puffer
}

TEST_CASE("Scheitert der Pipeline-Bau, bleibt die alte Pipeline aktiv") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    const gpu::ComputePipelineHandle good = fx.lastDispatch().pipeline;

    insertMix(fx);
    fx.device.failNextPipeline();
    const Result<void> result = fx.stage->setGraph(fx.lab.graph);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::ShaderError);
    REQUIRE(fx.stage->graphError().has_value());
    CHECK(fx.stage->graphError()->code == ErrorCode::ShaderError);

    CHECK(fx.device.isPipelineAlive(good));
    CHECK(fx.device.livePipelineCount() == 1U);
    CHECK(fx.device.liveBufferCount() == 2U);
    fx.render(1.0);
    CHECK(fx.lastDispatch().pipeline == good);

    // Ein zweiter Versuch gelingt und löst die alte Pipeline ab.
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK_FALSE(fx.stage->graphError().has_value());
    CHECK_FALSE(fx.device.isPipelineAlive(good));
    CHECK(fx.device.livePipelineCount() == 1U);
}

TEST_CASE("Scheitert schon der erste Pipeline-Bau, gibt es nichts zu zeichnen") {
    StageFixture fx;
    fx.device.failNextPipeline();
    REQUIRE_FALSE(fx.stage->setGraph(fx.lab.graph).ok());

    fx.render(0.0);
    CHECK(fx.device.dispatches().empty());
    CHECK(fx.stage->stats().framesSkipped == 1U);
    CHECK(fx.device.livePipelineCount() == 0U);
    CHECK(fx.device.liveBufferCount() == 1U);

    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(1.0);
    CHECK(fx.device.dispatches().size() == 1U);
}

TEST_CASE("Scheitert der Parameterpuffer beim ersten Bau, bleibt nichts liegen") {
    StageFixture fx;
    fx.device.failNextBuffer();
    const Result<void> result = fx.stage->setGraph(fx.lab.graph);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::GpuError);
    CHECK(fx.device.livePipelineCount() == 0U);  // die frisch gebaute Pipeline wird zurückgenommen
    CHECK(fx.device.liveBufferCount() == 1U);
}

TEST_CASE("Scheitert der neue Parameterpuffer, bleibt der alte Zustand vollständig") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    const gpu::ComputePipelineHandle good = fx.lastDispatch().pipeline;

    insertMix(fx);  // zweiter Gradient: größerer Parameterpuffer
    fx.device.failNextBuffer();
    REQUIRE_FALSE(fx.stage->setGraph(fx.lab.graph).ok());

    CHECK(fx.device.livePipelineCount() == 1U);
    CHECK(fx.device.isPipelineAlive(good));
    CHECK(fx.device.liveBufferCount() == 2U);
    fx.render(1.0);
    CHECK(fx.lastDispatch().pipeline == good);
    CHECK(fx.stage->stats().pipelinesBuilt == 1U);
}
