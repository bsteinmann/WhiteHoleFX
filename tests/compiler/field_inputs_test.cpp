// Werte von außen im Feldcompiler: Modulation von Parametern und Konstanten an Field-Eingängen.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "fake_value_lookup.hpp"
#include "test_field_nodes.hpp"
#include "whitehole/compiler/field_compiler.hpp"

using namespace whitehole;
using namespace whitehole::compiler;
using whitehole::test::CompilerFixture;
using whitehole::test::FakeValueLookup;
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

float floatAt(const std::vector<std::byte>& bytes, std::uint32_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

const ParameterSlot* slotFor(const ParameterLayout& layout, NodeId node, std::string_view key) {
    for (const ParameterSlot& slot : layout.slots) {
        if (slot.node == node && slot.key == key) {
            return &slot;
        }
    }
    return nullptr;
}

/// c.const -> c.knob; liefert den knob.
NodeId makeKnob(graph::Graph& graph) {
    const NodeId source = add(graph, "c.const");
    const NodeId knob = add(graph, "c.knob");
    link(graph, source, knob, "source");
    return knob;
}

}  // namespace

TEST_CASE("Modulation: Verbinden ändert weder Shader noch Strukturhash") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId knob = makeKnob(graph);

    const auto before = compileField(graph, fixture.library, out(knob));
    REQUIRE(before.ok());

    link(graph, add(graph, "c.scalar"), knob, "amount");
    link(graph, add(graph, "c.flag"), knob, "flag");
    link(graph, add(graph, "c.color"), knob, "tint");

    const auto after = compileField(graph, fixture.library, out(knob));
    REQUIRE(after.ok());
    CHECK(after.value().wgsl == before.value().wgsl);
    CHECK(after.value().structureHash == before.value().structureHash);
    CHECK(after.value().nodes.size() == 2U);  // die CPU-Quellen werden nicht übersetzt
}

TEST_CASE("Modulation: packParameters nimmt den Wert der Quelle") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId knob = makeKnob(graph);
    const NodeId scalar = add(graph, "c.scalar");
    link(graph, scalar, knob, "amount");

    const auto compiled = compileField(graph, fixture.library, out(knob));
    REQUIRE(compiled.ok());
    const ParameterSlot* slot = slotFor(compiled.value().layout, knob, "amount");
    REQUIRE(slot != nullptr);
    CHECK(slot->origin == SlotOrigin::Parameter);

    FakeValueLookup lookup;

    SECTION("ohne Lookup gilt der gespeicherte Wert") {
        CHECK(floatAt(packParameters(graph, compiled.value().layout), slot->offsetBytes) == 0.5F);
        CHECK(floatAt(packParameters(graph, compiled.value().layout, nullptr), slot->offsetBytes) == 0.5F);
    }
    SECTION("Lookup ohne Wert für die Quelle: gespeicherter Wert") {
        const auto bytes = packParameters(graph, compiled.value().layout, &lookup);
        CHECK(floatAt(bytes, slot->offsetBytes) == 0.5F);
    }
    SECTION("Wert der Quelle") {
        lookup.set(scalar, "out", 0.25);
        CHECK(floatAt(packParameters(graph, compiled.value().layout, &lookup), slot->offsetBytes) == 0.25F);
    }
    SECTION("Wert wird auf den Bereich des Parameters begrenzt") {
        lookup.set(scalar, "out", 7.0);
        CHECK(floatAt(packParameters(graph, compiled.value().layout, &lookup), slot->offsetBytes) == 1.0F);
        lookup.set(scalar, "out", -3.0);
        CHECK(floatAt(packParameters(graph, compiled.value().layout, &lookup), slot->offsetBytes) == 0.0F);
    }
    SECTION("nicht endlicher oder falsch typisierter Wert: gespeicherter Wert") {
        lookup.set(scalar, "out", std::numeric_limits<double>::quiet_NaN());
        CHECK(floatAt(packParameters(graph, compiled.value().layout, &lookup), slot->offsetBytes) == 0.5F);
        lookup.set(scalar, "out", std::string{"text"});
        CHECK(floatAt(packParameters(graph, compiled.value().layout, &lookup), slot->offsetBytes) == 0.5F);
    }
    SECTION("nach dem Trennen gilt wieder der gespeicherte Wert") {
        lookup.set(scalar, "out", 0.9);
        const graph::Connection* connection = graph.findInputConnection(knob, "amount");
        REQUIRE(connection != nullptr);
        REQUIRE(graph.disconnect(connection->id).ok());
        CHECK(floatAt(packParameters(graph, compiled.value().layout, &lookup), slot->offsetBytes) == 0.5F);
    }
}

TEST_CASE("Modulation: Boolean und Farbe") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId knob = makeKnob(graph);
    const NodeId flag = add(graph, "c.flag");
    const NodeId color = add(graph, "c.color");
    link(graph, flag, knob, "flag");
    link(graph, color, knob, "tint");

    const auto compiled = compileField(graph, fixture.library, out(knob));
    REQUIRE(compiled.ok());
    const ParameterSlot* flagSlot = slotFor(compiled.value().layout, knob, "flag");
    const ParameterSlot* tintSlot = slotFor(compiled.value().layout, knob, "tint");
    REQUIRE(flagSlot != nullptr);
    REQUIRE(tintSlot != nullptr);

    FakeValueLookup lookup;
    lookup.set(flag, "out", false);
    lookup.set(color, "out", ColorRgba{0.1F, 0.2F, 0.3F, 0.4F});

    const auto bytes = packParameters(graph, compiled.value().layout, &lookup);
    CHECK(floatAt(bytes, flagSlot->offsetBytes) == 0.0F);  // gespeichert wäre true
    CHECK(floatAt(bytes, tintSlot->offsetBytes) == 0.1F);
    CHECK(floatAt(bytes, tintSlot->offsetBytes + 12U) == 0.4F);
}

TEST_CASE("Konstante: Skalar an einem Field-Eingang") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId scalar = add(graph, "c.scalar");
    const NodeId shift = add(graph, "c.shift");
    link(graph, scalar, shift, "source");

    const auto compiled = compileField(graph, fixture.library, out(shift));
    REQUIRE(compiled.ok());
    const CompiledField& result = compiled.value();

    const std::string member = "wh_params." + memberName(shift, "source");
    CHECK(contains(result.wgsl, "vec4<f32>(" + member + ", " + member + ", " + member + ", 1.0)"));
    CHECK(contains(result.wgsl, memberName(shift, "source") + ": f32,"));
    CHECK_FALSE(contains(result.wgsl, "wh_f" + std::to_string(scalar.value())));
    CHECK(result.nodes == std::vector<NodeId>{shift});

    const ParameterSlot* slot = slotFor(result.layout, shift, "source");
    REQUIRE(slot != nullptr);
    CHECK(slot->origin == SlotOrigin::FieldConstant);
    CHECK(slot->type == GpuType::Float);

    FakeValueLookup lookup;
    CHECK(floatAt(packParameters(graph, result.layout, &lookup), slot->offsetBytes) == 0.0F);
    CHECK(floatAt(packParameters(graph, result.layout), slot->offsetBytes) == 0.0F);

    lookup.set(scalar, "out", 0.75);
    CHECK(floatAt(packParameters(graph, result.layout, &lookup), slot->offsetBytes) == 0.75F);

    // Ein ganzzahliger Wert funktioniert ebenfalls (Integer lässt sich als Scalar verbinden).
    lookup.set(scalar, "out", std::int64_t{3});
    CHECK(floatAt(packParameters(graph, result.layout, &lookup), slot->offsetBytes) == 3.0F);
}

TEST_CASE("Konstante: Farbe an einem Field-Eingang") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId base = add(graph, "c.const");
    const NodeId color = add(graph, "c.color");
    const NodeId sum = add(graph, "c.add");
    link(graph, base, sum, "a");
    link(graph, color, sum, "b");

    const auto compiled = compileField(graph, fixture.library, out(sum));
    REQUIRE(compiled.ok());
    const CompiledField& result = compiled.value();

    CHECK(contains(result.wgsl, "return wh_f1(p) + wh_params." + memberName(sum, "b") + ";"));
    CHECK(contains(result.wgsl, memberName(sum, "b") + ": vec4<f32>,"));

    const ParameterSlot* slot = slotFor(result.layout, sum, "b");
    REQUIRE(slot != nullptr);
    CHECK(slot->type == GpuType::Vec4);
    CHECK(slot->origin == SlotOrigin::FieldConstant);
    CHECK(slot->offsetBytes % 16U == 0U);

    FakeValueLookup lookup;
    lookup.set(color, "out", ColorRgba{0.5F, 0.25F, 0.125F, 1.0F});
    const auto bytes = packParameters(graph, result.layout, &lookup);
    CHECK(floatAt(bytes, slot->offsetBytes) == 0.5F);
    CHECK(floatAt(bytes, slot->offsetBytes + 4U) == 0.25F);
    CHECK(floatAt(bytes, slot->offsetBytes + 8U) == 0.125F);
    CHECK(floatAt(bytes, slot->offsetBytes + 12U) == 1.0F);
}

TEST_CASE("Konstante: der Layout-Offset aller Slots bleibt WGSL-konform ausgerichtet") {
    const CompilerFixture fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId sum = add(graph, "c.add");
    link(graph, add(graph, "c.scalar"), sum, "a");  // Float
    link(graph, add(graph, "c.color"), sum, "b");   // vec4 danach: braucht 16er-Ausrichtung

    const auto compiled = compileField(graph, fixture.library, out(sum));
    REQUIRE(compiled.ok());
    const ParameterSlot* floatSlot = slotFor(compiled.value().layout, sum, "a");
    const ParameterSlot* vecSlot = slotFor(compiled.value().layout, sum, "b");
    REQUIRE(floatSlot != nullptr);
    REQUIRE(vecSlot != nullptr);
    CHECK(floatSlot->offsetBytes == 0U);
    CHECK(vecSlot->offsetBytes == 16U);
    CHECK(compiled.value().layout.sizeBytes % 16U == 0U);
}

TEST_CASE("Konstante: Wechsel zwischen Konstante und Feld ändert den Strukturhash") {
    const CompilerFixture fixture = makeCompilerFixture();

    graph::Graph withConstant{fixture.registry};
    const NodeId shiftA = add(withConstant, "c.shift");
    link(withConstant, add(withConstant, "c.scalar"), shiftA, "source");

    graph::Graph withField{fixture.registry};
    const NodeId shiftB = add(withField, "c.shift");
    link(withField, add(withField, "c.const"), shiftB, "source");

    const auto a = compileField(withConstant, fixture.library, out(shiftA));
    const auto b = compileField(withField, fixture.library, out(shiftB));
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(a.value().structureHash != b.value().structureHash);
}
