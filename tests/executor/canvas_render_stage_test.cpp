// CanvasRenderStage: Erzeugen, Lebensdauer der Ressourcen und der Inhalt eines Frames.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "stage_fixture.hpp"
#include "test_cpu_nodes.hpp"

using namespace whitehole;
using namespace whitehole::test;

TEST_CASE("create legt Textur und Frame-Puffer an, aber noch keine Pipeline") {
    StageFixture fx{Extent3{32, 16, 1}, gpu::TextureFormat::Rgba8Unorm};

    CHECK(fx.device.liveTextureCount() == 1U);
    CHECK(fx.device.liveBufferCount() == 1U);
    CHECK(fx.device.livePipelineCount() == 0U);

    const gpu::TextureDesc& desc = fx.device.textureDescs().back();
    CHECK(desc.extent.width == 32U);
    CHECK(desc.extent.height == 16U);
    CHECK(desc.format == gpu::TextureFormat::Rgba8Unorm);
    CHECK(gpu::hasFlag(desc.usage, gpu::Usage::Storage));
    CHECK(gpu::hasFlag(desc.usage, gpu::Usage::Sampled));
    CHECK(gpu::hasFlag(desc.usage, gpu::Usage::CopySource));
    CHECK_FALSE(fx.stage->graphError().has_value());
    CHECK_FALSE(fx.stage->frameError().has_value());
}

TEST_CASE("create lehnt ungeeignete Einstellungen ab, ohne Ressourcen zu hinterlassen") {
    FakeGpuDevice device;
    Lab lab;
    executor::CanvasStageConfig config;
    config.extent = Extent3{8, 8, 1};

    SECTION("Größe 0") {
        config.extent = Extent3{0, 8, 1};
        const auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE_FALSE(created.ok());
        CHECK(created.error().code == ErrorCode::InvalidArgument);
    }
    SECTION("Höhe 0") {
        config.extent = Extent3{8, 0, 1};
        const auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE_FALSE(created.ok());
        CHECK(created.error().code == ErrorCode::InvalidArgument);
    }
    SECTION("Tiefe größer 1") {
        config.extent = Extent3{8, 8, 4};
        const auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE_FALSE(created.ok());
        CHECK(created.error().code == ErrorCode::Unsupported);
    }
    SECTION("Format ohne RGBA-Storage") {
        config.format = gpu::TextureFormat::R32Float;
        const auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE_FALSE(created.ok());
        CHECK(created.error().code == ErrorCode::InvalidArgument);
    }
    CHECK(device.liveTextureCount() == 0U);
    CHECK(device.liveBufferCount() == 0U);
}

TEST_CASE("Scheitert das Anlegen des Frame-Puffers, wird die Textur wieder freigegeben") {
    FakeGpuDevice device;
    Lab lab;
    executor::CanvasStageConfig config;
    config.extent = Extent3{8, 8, 1};
    device.failNextBuffer();

    const auto created = executor::CanvasRenderStage::create(device, lab.snippets, lab.kernels, config);
    REQUIRE_FALSE(created.ok());
    CHECK(created.error().code == ErrorCode::GpuError);
    CHECK(device.liveTextureCount() == 0U);
    CHECK(device.liveBufferCount() == 0U);
}

TEST_CASE("Ohne Graph zeichnet record nichts auf") {
    StageFixture fx;
    fx.render(1.0);
    fx.render(2.0);

    CHECK(fx.device.dispatches().empty());
    CHECK(fx.stage->stats().framesSkipped == 2U);
    CHECK(fx.stage->stats().framesRecorded == 0U);
}

TEST_CASE("Ein Frame ist ein Dispatch mit drei Bindungen") {
    StageFixture fx{Extent3{16, 8, 1}};
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(1.0);

    REQUIRE(fx.device.dispatches().size() == 1U);
    const auto& dispatch = fx.lastDispatch();
    REQUIRE(dispatch.bindings.size() == 3U);
    CHECK(dispatch.pipeline.value() != 0U);
    CHECK(fx.device.isPipelineAlive(dispatch.pipeline));

    CHECK(bufferAt(dispatch, 0, 0).value() != 0U);  // Frame-Daten
    CHECK(bufferAt(dispatch, 0, 1).value() != 0U);  // Parameter
    CHECK(textureAt(dispatch, 1, 0) == fx.stage->output());
    CHECK(fx.stage->stats().framesRecorded == 1U);
    CHECK(fx.stage->stats().framesSkipped == 0U);
}

TEST_CASE("Die Arbeitsgruppen decken die Textur ab, auch bei krummen Größen") {
    struct Case {
        std::uint32_t width, height, groupsX, groupsY;
    };
    const Case cases[] = {{16, 8, 2, 1}, {17, 9, 3, 2}, {1, 1, 1, 1}, {8, 8, 1, 1}, {9, 8, 2, 1}, {64, 33, 8, 5}};
    for (const Case& c : cases) {
        CAPTURE(c.width, c.height);
        StageFixture fx{Extent3{c.width, c.height, 1}};
        REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
        fx.render(0.0);
        CHECK(fx.lastDispatch().groupsX == c.groupsX);
        CHECK(fx.lastDispatch().groupsY == c.groupsY);
        CHECK(fx.lastDispatch().groupsZ == 1U);
    }
}

TEST_CASE("Die Pipeline enthält Feldbibliothek, Einstieg und das gewählte Format") {
    StageFixture fx{Extent3{8, 8, 1}, gpu::TextureFormat::Rgba32Float};
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    REQUIRE(fx.device.pipelineDescs().size() == 1U);
    const gpu::ComputePipelineDesc& desc = fx.device.pipelineDescs().back();
    CHECK(desc.shader.language == gpu::ShaderLanguage::Wgsl);
    CHECK(desc.shader.entryPoint == "main");
    CHECK(desc.shader.code.find("fn wh_field(") != std::string::npos);
    CHECK(desc.shader.code.find("texture_storage_2d<rgba32float, write>") != std::string::npos);
}

TEST_CASE("Frame-Daten: Zeit, Delta und Frame-Nummer kommen an") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(0.0);
    fx.render(1.0);
    fx.render(2.5, 0.25);

    const compiler::FrameUniforms frame = frameOf(fx);
    CHECK(frame.time == 2.5F);
    CHECK(frame.delta == 0.25F);
    CHECK(frame.frame == 2U);
}

TEST_CASE("Durchstich: Time steuert Rotate.angle im Parameterpuffer") {
    StageFixture fx;
    const NodeId time = fx.lab.add("wh.value.time");
    fx.lab.connect(time, "seconds", fx.rotate, "angle");
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    const auto compiled = compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target());
    REQUIRE(compiled.ok());
    const compiler::ParameterSlot* angle = slotFor(compiled.value().layout, fx.rotate, "angle");
    REQUIRE(angle != nullptr);

    fx.render(0.0);
    CHECK(floatAt(parametersOf(fx), angle->offsetBytes) == 0.0F);
    fx.render(10.0);
    CHECK(floatAt(parametersOf(fx), angle->offsetBytes) == 10.0F);
    fx.render(20.0);
    CHECK(floatAt(parametersOf(fx), angle->offsetBytes) == 20.0F);

    CHECK(fx.stage->stats().pipelinesBuilt == 1U);  // Zeit baut nie neu
    CHECK_FALSE(fx.stage->frameError().has_value());
}

TEST_CASE("Der Parameterpuffer entspricht packParameters mit den ausgewerteten Werten") {
    StageFixture fx;
    const NodeId time = fx.lab.add("wh.value.time");
    fx.lab.connect(time, "seconds", fx.rotate, "angle");
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(7.0);

    const auto compiled = compiler::compileField(fx.lab.graph, fx.lab.snippets, fx.target());
    REQUIRE(compiled.ok());
    values::ValueStore store;
    values::ValueEvaluator evaluator{fx.lab.kernels};
    FrameTime t;
    t.time = Seconds{7.0};
    REQUIRE(evaluator.evaluate(fx.lab.graph, t, store).ok());

    CHECK(parametersOf(fx) == compiler::packParameters(fx.lab.graph, compiled.value().layout, &store));
    CHECK(parametersOf(fx).size() == compiled.value().layout.sizeBytes);
}

TEST_CASE("Ein Fehler im Werte-Graphen lässt den Frame trotzdem laufen") {
    StageFixture fx;
    auto probes = std::make_shared<KernelProbes>();
    std::vector<values::CpuNodeDefinition> definitions;
    definitions.push_back(detail::definition(
        "v.badtype", {outPort("out", graph::PortType::Scalar)}, {},
        detail::stateless(probes, [](const values::KernelContext&, values::KernelOutputs& o) {
            o.set("out", std::string{"kein Scalar"});
        })));
    REQUIRE(values::registerCpuDefinitions(definitions, *fx.lab.registry, fx.lab.kernels).ok());

    const NodeId bad = fx.lab.add("v.badtype");
    fx.lab.connect(bad, "out", fx.rotate, "angle");
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    fx.render(1.0);
    REQUIRE(fx.stage->frameError().has_value());
    CHECK(fx.stage->frameError()->code == ErrorCode::InvalidArgument);
    CHECK(fx.device.dispatches().size() == 1U);  // der Frame ist trotzdem aufgezeichnet
    CHECK(fx.stage->stats().framesRecorded == 1U);

    // Der nächste Frame meldet den Fehler erneut; ein repariertes Netz meldet nichts mehr.
    fx.render(2.0);
    CHECK(fx.stage->frameError().has_value());
    REQUIRE(fx.lab.graph.removeNode(bad).ok());
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    fx.render(3.0);
    CHECK_FALSE(fx.stage->frameError().has_value());
}

TEST_CASE("Scheitert ein Upload, entfällt der Frame und der Fehler wird gemeldet") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    SECTION("Parameterpuffer (erster Upload)") {
        fx.device.failWriteAfter(0);
    }
    SECTION("Frame-Daten (zweiter Upload)") {
        fx.device.failWriteAfter(1);
    }
    fx.render(0.0);

    REQUIRE(fx.stage->frameError().has_value());
    CHECK(fx.stage->frameError()->code == ErrorCode::GpuError);
    CHECK(fx.device.dispatches().empty());
    CHECK(fx.stage->stats().framesSkipped == 1U);
    CHECK(fx.stage->stats().framesRecorded == 0U);

    // Der nächste Frame läuft wieder.
    fx.render(1.0);
    CHECK_FALSE(fx.stage->frameError().has_value());
    CHECK(fx.device.dispatches().size() == 1U);
}

TEST_CASE("Die Stage gibt alle Ressourcen frei") {
    StageFixture fx;
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK(fx.device.livePipelineCount() == 1U);
    CHECK(fx.device.liveBufferCount() == 2U);
    CHECK(fx.device.liveTextureCount() == 1U);

    fx.stage.reset();
    CHECK(fx.device.livePipelineCount() == 0U);
    CHECK(fx.device.liveBufferCount() == 0U);
    CHECK(fx.device.liveTextureCount() == 0U);
}
