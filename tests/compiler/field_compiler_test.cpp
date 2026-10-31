// Tests von compileField: aus Field-Nodes entsteht eine WGSL-Funktion mit Bindings und Parameterlayout.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "test_field_nodes.hpp"
#include "whitehole/compiler/field_compiler.hpp"

using namespace whitehole;
using namespace whitehole::compiler;
using whitehole::test::CompilerFixture;
using whitehole::test::makeCompilerFixture;

namespace {

NodeId add(graph::Graph& graph, std::string_view typeId) {
    return graph.addNode(typeId, Vec2{}).value();
}

graph::PortRef out(NodeId node) { return graph::PortRef{node, "out"}; }

void link(graph::Graph& graph, NodeId from, NodeId to, const std::string& port) {
    REQUIRE(graph.connect(out(from), graph::PortRef{to, port}).ok());
}

bool contains(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

std::size_t count(const std::string& text, std::string_view part) {
    std::size_t result = 0;
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) {
        ++result;
    }
    return result;
}

float floatAt(const std::vector<std::byte>& bytes, std::uint32_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

}  // namespace

TEST_CASE("compileField: einzelner Node mit Bindings, Layout und Einstiegsfunktion") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = add(graph, "c.const");

    const auto compiled = compileField(graph, fixture.library, out(node));
    REQUIRE(compiled.ok());
    const CompiledField& result = compiled.value();

    CHECK(contains(result.wgsl, "fn wh_f1(p: vec3<f32>) -> vec4<f32> {"));
    CHECK(contains(result.wgsl, "    return wh_params.n1_color * wh_params.n1_level;\n}"));
    CHECK(contains(result.wgsl, "fn wh_field(p: vec3<f32>) -> vec4<f32> {\n    return wh_f1(p);\n}"));
    CHECK(contains(result.wgsl, "@group(0) @binding(0) var<uniform> wh_frame: WhFrame;"));
    CHECK(contains(result.wgsl, "@group(0) @binding(1) var<uniform> wh_params: WhParams;"));

    // Der Text-Parameter "label" kommt nicht in den Puffer.
    REQUIRE(result.layout.slots.size() == 5U);
    const std::vector<std::uint32_t> offsets{0, 16, 24, 32, 44};
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        CHECK(result.layout.slots[i].offsetBytes == offsets[i]);
    }
    CHECK(result.layout.sizeBytes == 64U);
    CHECK(result.nodes == std::vector<NodeId>{node});
    CHECK(result.structureHash != 0U);
}

TEST_CASE("compileField: Eingänge stehen vor ihren Verbrauchern, Koordinate wird durchgereicht") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId source = add(graph, "c.const");
    const NodeId shift = add(graph, "c.shift");
    link(graph, source, shift, "source");

    const auto compiled = compileField(graph, fixture.library, out(shift));
    REQUIRE(compiled.ok());
    const CompiledField& result = compiled.value();

    CHECK(result.wgsl.find("fn wh_f1(") < result.wgsl.find("fn wh_f2("));
    CHECK(contains(result.wgsl,
                   "    return wh_f1(p + vec3<f32>(wh_params.n2_amount, 0.0, 0.0));\n}"));
    CHECK(contains(result.wgsl, "    return wh_f2(p);\n}"));

    const std::vector<NodeId> expectedNodes{source, shift};
    CHECK(result.nodes == expectedNodes);

    REQUIRE(result.layout.slots.size() == 6U);
    CHECK(result.layout.slots.back().member == "n2_amount");
    CHECK(result.layout.slots.back().offsetBytes == 48U);
    CHECK(result.layout.sizeBytes == 80U);
}

TEST_CASE("compileField: ein nicht verbundener optionaler Eingang liefert das Nullfeld") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId source = add(graph, "c.const");
    const NodeId sum = add(graph, "c.add");
    link(graph, source, sum, "a");

    const auto compiled = compileField(graph, fixture.library, out(sum));
    REQUIRE(compiled.ok());
    CHECK(contains(compiled.value().wgsl,
                   "return wh_f1(p) + vec4<f32>(0.0, 0.0, 0.0, 0.0);"));
}

TEST_CASE("compileField: ein nicht verbundener Pflicht-Eingang ist ein Fehler") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId shift = add(graph, "c.shift");

    const auto compiled = compileField(graph, fixture.library, out(shift));
    REQUIRE_FALSE(compiled.ok());
    CHECK(compiled.error().code == ErrorCode::InvalidArgument);
    CHECK(contains(compiled.error().message, "source"));
    CHECK(contains(compiled.error().message, "c.shift"));
}

TEST_CASE("compileField: ein von zwei Verbrauchern genutzter Node wird nur einmal erzeugt") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId source = add(graph, "c.const");
    const NodeId sum = add(graph, "c.add");
    link(graph, source, sum, "a");
    link(graph, source, sum, "b");

    const auto compiled = compileField(graph, fixture.library, out(sum));
    REQUIRE(compiled.ok());
    const std::string& wgsl = compiled.value().wgsl;
    CHECK(count(wgsl, "fn wh_f1(") == 1U);
    CHECK(contains(wgsl, "return wh_f1(p) + wh_f1(p);"));
    CHECK(compiled.value().nodes.size() == 2U);
}

TEST_CASE("compileField: gleiche Preludes werden zusammengelegt") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId first = add(graph, "c.noise");
    const NodeId second = add(graph, "c.noise2");
    const NodeId sum = add(graph, "c.add");
    link(graph, first, sum, "a");
    link(graph, second, sum, "b");

    const auto compiled = compileField(graph, fixture.library, out(sum));
    REQUIRE(compiled.ok());
    const std::string& wgsl = compiled.value().wgsl;
    CHECK(count(wgsl, "fn c_hash(") == 1U);
    CHECK(wgsl.find("fn c_hash(") < wgsl.find("fn wh_f1("));  // Prelude steht vor den Node-Funktionen
    CHECK(contains(wgsl, "return vec4<f32>(c_hash(p.x + wh_frame.time));"));
}

TEST_CASE("compileField: nicht verbundene Nodes des Graphen bleiben außen vor") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId used = add(graph, "c.const");
    add(graph, "c.noise");

    const auto compiled = compileField(graph, fixture.library, out(used));
    REQUIRE(compiled.ok());
    CHECK(compiled.value().nodes.size() == 1U);
    CHECK_FALSE(contains(compiled.value().wgsl, "wh_f2"));
    CHECK_FALSE(contains(compiled.value().wgsl, "c_hash"));
}

TEST_CASE("compileField: nicht unterstützte Konstellationen melden Unsupported") {
    const CompilerFixture fixture = makeCompilerFixture();

    SECTION("Node außerhalb der Field-Domäne") {
        graph::Graph graph{fixture.registry};
        const NodeId raster = add(graph, "c.raster");
        const auto compiled = compileField(graph, fixture.library, out(raster));
        REQUIRE_FALSE(compiled.ok());
        CHECK(compiled.error().code == ErrorCode::Unsupported);
    }
    SECTION("Node mit zwei Field-Ausgängen") {
        graph::Graph graph{fixture.registry};
        const NodeId node = add(graph, "c.twoouts");
        const auto compiled = compileField(graph, fixture.library, out(node));
        REQUIRE_FALSE(compiled.ok());
        CHECK(compiled.error().code == ErrorCode::Unsupported);
    }
    SECTION("verbundener Skalar-Eingang ohne gleichnamigen Parameter") {
        graph::Graph graph{fixture.registry};
        const NodeId source = add(graph, "c.const");
        const NodeId scalar = add(graph, "c.scalar");
        const NodeId modulated = add(graph, "c.modulated");
        link(graph, source, modulated, "source");
        link(graph, scalar, modulated, "amount");
        const auto compiled = compileField(graph, fixture.library, out(modulated));
        REQUIRE_FALSE(compiled.ok());
        CHECK(compiled.error().code == ErrorCode::Unsupported);
        CHECK(contains(compiled.error().message, "amount"));
    }
    SECTION("Bild als Feld-Eingang") {
        graph::Graph graph{fixture.registry};
        const NodeId image = add(graph, "c.image");
        const NodeId shift = add(graph, "c.shift");
        link(graph, image, shift, "source");
        const auto compiled = compileField(graph, fixture.library, out(shift));
        REQUIRE_FALSE(compiled.ok());
        CHECK(compiled.error().code == ErrorCode::Unsupported);
    }
}

TEST_CASE("compileField: fehlendes oder fehlerhaftes Snippet") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};

    const auto missing = compileField(graph, fixture.library, out(add(graph, "c.nosnippet")));
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().code == ErrorCode::NotFound);

    const auto badPort = compileField(graph, fixture.library, out(add(graph, "c.badbody")));
    REQUIRE_FALSE(badPort.ok());
    CHECK(badPort.error().code == ErrorCode::InvalidArgument);
    CHECK(contains(badPort.error().message, "nope"));

    const auto badParameter = compileField(graph, fixture.library, out(add(graph, "c.badparam")));
    REQUIRE_FALSE(badParameter.ok());
    CHECK(badParameter.error().code == ErrorCode::InvalidArgument);
    CHECK(contains(badParameter.error().message, "label"));
}

TEST_CASE("compileField: ungültige Ziele") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId shift = add(graph, "c.shift");
    const NodeId scalar = add(graph, "c.scalar");

    CHECK(compileField(graph, fixture.library, out(NodeId{99})).error().code == ErrorCode::NotFound);
    CHECK(compileField(graph, fixture.library, graph::PortRef{shift, "nope"}).error().code ==
          ErrorCode::NotFound);
    CHECK(compileField(graph, fixture.library, graph::PortRef{shift, "source"}).error().code ==
          ErrorCode::InvalidArgument);  // Eingang statt Ausgang
    CHECK(compileField(graph, fixture.library, out(scalar)).error().code ==
          ErrorCode::InvalidArgument);  // Ausgang, aber kein Field
}

TEST_CASE("Strukturhash bleibt bei Parameteränderungen gleich und ändert sich bei Strukturänderungen") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId source = add(graph, "c.const");
    const NodeId shift = add(graph, "c.shift");
    link(graph, source, shift, "source");

    const auto before = compileField(graph, fixture.library, out(shift));
    REQUIRE(before.ok());
    const std::vector<std::byte> bytesBefore = packParameters(graph, before.value().layout);

    REQUIRE(graph.setParameter(shift, "amount", 0.75).ok());
    REQUIRE(graph.setParameter(source, "level", 0.25).ok());

    const auto after = compileField(graph, fixture.library, out(shift));
    REQUIRE(after.ok());
    CHECK(after.value().wgsl == before.value().wgsl);
    CHECK(after.value().structureHash == before.value().structureHash);
    CHECK(packParameters(graph, after.value().layout) != bytesBefore);

    // Struktur ändern: ein weiterer Node hinter shift
    const NodeId second = add(graph, "c.shift");
    link(graph, shift, second, "source");
    const auto longer = compileField(graph, fixture.library, out(second));
    REQUIRE(longer.ok());
    CHECK(longer.value().structureHash != before.value().structureHash);
}

TEST_CASE("packParameters schreibt die Werte an die Offsets des Layouts") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = add(graph, "c.const");

    const auto compiled = compileField(graph, fixture.library, out(node));
    REQUIRE(compiled.ok());
    const ParameterLayout& layout = compiled.value().layout;

    // Standardwerte: color (1,1,1,1) @0, level 1 @16, offset (0.1,0.2) @24, dir @32, flag true @44
    std::vector<std::byte> bytes = packParameters(graph, layout);
    REQUIRE(bytes.size() == layout.sizeBytes);
    CHECK(floatAt(bytes, 0) == 1.0F);
    CHECK(floatAt(bytes, 16) == 1.0F);
    CHECK(floatAt(bytes, 24) == 0.1F);
    CHECK(floatAt(bytes, 28) == 0.2F);
    CHECK(floatAt(bytes, 32) == 0.3F);
    CHECK(floatAt(bytes, 40) == 0.5F);
    CHECK(floatAt(bytes, 44) == 1.0F);  // bool true

    REQUIRE(graph.setParameter(node, "color", ColorRgba{0.1F, 0.2F, 0.3F, 0.4F}).ok());
    REQUIRE(graph.setParameter(node, "level", 0.5).ok());
    REQUIRE(graph.setParameter(node, "dir", Vec3{3.0F, 4.0F, 5.0F}).ok());
    REQUIRE(graph.setParameter(node, "flag", false).ok());
    REQUIRE(graph.setParameter(node, "label", std::string{"ignored"}).ok());

    bytes = packParameters(graph, layout);
    CHECK(floatAt(bytes, 0) == 0.1F);
    CHECK(floatAt(bytes, 4) == 0.2F);
    CHECK(floatAt(bytes, 8) == 0.3F);
    CHECK(floatAt(bytes, 12) == 0.4F);
    CHECK(floatAt(bytes, 16) == 0.5F);
    CHECK(floatAt(bytes, 32) == 3.0F);
    CHECK(floatAt(bytes, 36) == 4.0F);
    CHECK(floatAt(bytes, 40) == 5.0F);
    CHECK(floatAt(bytes, 44) == 0.0F);  // bool false
    for (std::uint32_t offset = 48; offset < layout.sizeBytes; offset += 4) {
        CHECK(floatAt(bytes, offset) == 0.0F);  // Abschlusselement bleibt null
    }
}

TEST_CASE("packParameters: ein inzwischen entfernter Node lässt seine Bytes null") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = add(graph, "c.const");
    const auto compiled = compileField(graph, fixture.library, out(node));
    REQUIRE(compiled.ok());

    REQUIRE(graph.removeNode(node).ok());
    const std::vector<std::byte> bytes = packParameters(graph, compiled.value().layout);
    REQUIRE(bytes.size() == compiled.value().layout.sizeBytes);
    for (const std::byte b : bytes) {
        CHECK(std::to_integer<unsigned>(b) == 0U);
    }
}
