// Tests des Platzhalters @phase: Er wird im Expander aufgelöst, der Feldcompiler legt je (Node, Schlüssel) genau einen
// Phase-Slot an, und packParameters füllt ihn aus der Rate des Parameters.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "test_field_nodes.hpp"
#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/compiler/phase_tracker.hpp"
#include "whitehole/compiler/snippet_expander.hpp"

using namespace whitehole;
using namespace whitehole::compiler;
using whitehole::test::makeCompilerFixture;

namespace {

NodeId add(graph::Graph& graph, std::string_view typeId) { return graph.addNode(typeId, Vec2{}).value(); }

graph::PortRef out(NodeId node) { return graph::PortRef{node, "out"}; }

std::size_t count(const std::string& text, std::string_view part) {
    std::size_t result = 0;
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) {
        ++result;
    }
    return result;
}

const ParameterSlot* findSlot(const ParameterLayout& layout, NodeId node, const std::string& key) {
    for (const ParameterSlot& slot : layout.slots) {
        if (slot.node == node && slot.key == key) {
            return &slot;
        }
    }
    return nullptr;
}

float floatAt(const std::vector<std::byte>& bytes, std::uint32_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

}  // namespace

TEST_CASE("expandSnippet: @phase wird über die Bindings aufgelöst") {
    SnippetBindings bindings;
    bindings.time = "T";
    bindings.phase = [](std::string_view key) -> Result<std::string> { return "PH." + std::string{key}; };

    CHECK(expandSnippet("x = fract(@phase(speed)) + @time;", bindings).value() == "x = fract(PH.speed) + T;");
    CHECK(expandSnippet("@phase( spaced )", bindings).value() == "PH.spaced");
}

TEST_CASE("expandSnippet: @phase ohne Bindung oder ohne Schlüssel ist ein Fehler") {
    SnippetBindings none;
    const auto unsupported = expandSnippet("@phase(speed)", none);
    REQUIRE_FALSE(unsupported.ok());
    CHECK(unsupported.error().message.find("@phase") != std::string::npos);

    SnippetBindings bindings;
    bindings.phase = [](std::string_view key) -> Result<std::string> { return std::string{key}; };
    CHECK_FALSE(expandSnippet("@phase()", bindings).ok());
    CHECK_FALSE(expandSnippet("@phase", bindings).ok());
}

TEST_CASE("compileField: @phase legt je Parameter genau einen Phase-Slot an, auch bei mehrfacher Benutzung") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = add(graph, "c.runner");

    const auto compiled = compileField(graph, fixture.library, out(node));
    REQUIRE(compiled.ok());
    const CompiledField& result = compiled.value();

    const ParameterSlot* speed = findSlot(result.layout, node, phaseSlotKey("speed"));
    const ParameterSlot* drift = findSlot(result.layout, node, phaseSlotKey("drift"));
    REQUIRE(speed != nullptr);
    REQUIRE(drift != nullptr);
    CHECK(speed->origin == SlotOrigin::Phase);
    CHECK(speed->type == GpuType::Float);
    CHECK(speed->member == "n1_phase_speed");
    CHECK(drift->member == "n1_phase_drift");

    // Nur ein Struct-Member je Schlüssel, obwohl @phase(speed) zweimal im Rumpf steht.
    CHECK(count(result.wgsl, "n1_phase_speed: f32") == 1U);
    CHECK(count(result.wgsl, "wh_params.n1_phase_speed") == 2U);
    CHECK(count(result.wgsl, "n1_phase_drift: f32") == 1U);
    CHECK(count(result.wgsl, "@time") == 0U);

    // Der Parameter selbst behält seinen eigenen Slot (der Rumpf liest ihn auch direkt).
    const ParameterSlot* plain = findSlot(result.layout, node, "speed");
    REQUIRE(plain != nullptr);
    CHECK(plain->origin == SlotOrigin::Parameter);
}

TEST_CASE("compileField: Phase-Slots tragen die Namen nach Ausgabereihenfolge, nicht nach Node-Nummer") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    (void)add(graph, "c.const");                    // Nummer 1, gehört nicht zum Ziel
    const NodeId runner = add(graph, "c.runner");   // Nummer 2, im Shader aber der erste Node

    const auto compiled = compileField(graph, fixture.library, out(runner));
    REQUIRE(compiled.ok());
    const ParameterSlot* slot = findSlot(compiled.value().layout, runner, phaseSlotKey("speed"));
    REQUIRE(slot != nullptr);
    CHECK(slot->member == "n1_phase_speed");
    CHECK(compiled.value().wgsl.find("wh_params.n1_phase_speed") != std::string::npos);
    CHECK(compiled.value().wgsl.find("n2_phase") == std::string::npos);
}

TEST_CASE("compileField: @phase auf einen Text oder einen unbekannten Parameter ist ein Fehler") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};

    const auto text = compileField(graph, fixture.library, out(add(graph, "c.badphase")));
    REQUIRE_FALSE(text.ok());
    CHECK(text.error().code == ErrorCode::InvalidArgument);
    CHECK(text.error().message.find("@phase(label)") != std::string::npos);

    const auto unknown = compileField(graph, fixture.library, out(add(graph, "c.unknownphase")));
    REQUIRE_FALSE(unknown.ok());
    CHECK(unknown.error().message.find("@phase(nope)") != std::string::npos);
}

TEST_CASE("compileField: Reglerwerte ändern die Struktur nicht, die Phase-Slots bleiben") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = add(graph, "c.runner");

    const auto before = compileField(graph, fixture.library, out(node));
    REQUIRE(before.ok());
    REQUIRE(graph.setParameter(node, "speed", 12.0).ok());
    const auto after = compileField(graph, fixture.library, out(node));
    REQUIRE(after.ok());
    CHECK(after.value().structureHash == before.value().structureHash);
    CHECK(after.value().wgsl == before.value().wgsl);
}

TEST_CASE("packParameters: der Phase-Slot eines kompilierten Nodes folgt dem Regler stetig") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = add(graph, "c.runner");  // speed 3, drift 0,5

    const auto compiled = compileField(graph, fixture.library, out(node));
    REQUIRE(compiled.ok());
    const ParameterLayout& layout = compiled.value().layout;
    const std::uint32_t speedAt = findSlot(layout, node, phaseSlotKey("speed"))->offsetBytes;
    const std::uint32_t driftAt = findSlot(layout, node, phaseSlotKey("drift"))->offsetBytes;

    PhaseTracker tracker;
    auto pack = [&](double time) { return packParameters(graph, layout, nullptr, PhaseSource{&tracker, time}); };

    std::vector<std::byte> bytes = pack(10.0);
    CHECK(floatAt(bytes, speedAt) == 30.0F);
    CHECK(floatAt(bytes, driftAt) == 5.0F);

    REQUIRE(graph.setParameter(node, "speed", 10.0).ok());  // Regler von 3 auf 10
    bytes = pack(10.0);
    CHECK(floatAt(bytes, speedAt) == 30.0F);  // im Augenblick der Änderung kein Sprung
    CHECK(floatAt(bytes, driftAt) == 5.0F);   // der andere Regler bleibt unberührt
    bytes = pack(11.0);
    CHECK(floatAt(bytes, speedAt) == 40.0F);  // danach mit der neuen Rate
    CHECK(floatAt(bytes, driftAt) == 5.5F);
}
