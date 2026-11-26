// Namen im erzeugten Shader hängen an der Ausgabereihenfolge, nicht an den Node-Nummern (shader_text.hpp): Dieselbe
// Struktur mit anderen Nummern ergibt denselben Text und denselben structureHash, damit der Pipeline-Zwischenspeicher
// im Live-Betrieb trifft. Die Werte kommen trotzdem aus den richtigen Nodes.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "test_field_nodes.hpp"
#include "whitehole/compiler/field_compiler.hpp"

using namespace whitehole;
using namespace whitehole::compiler;
using whitehole::test::makeCompilerFixture;

namespace {

NodeId add(graph::Graph& graph, std::string_view typeId) { return graph.addNode(typeId, Vec2{}).value(); }

graph::PortRef out(NodeId node) { return graph::PortRef{node, "out"}; }

void link(graph::Graph& graph, NodeId from, NodeId to, const std::string& port) {
    REQUIRE(graph.connect(out(from), graph::PortRef{to, port}).ok());
}

const ParameterSlot* slotFor(const ParameterLayout& layout, NodeId node, std::string_view key) {
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

TEST_CASE("Shader-Namen: gleiche Struktur mit anderen Node-Nummern ergibt denselben Shader", "[compiler][shader_names]") {
    const auto fixture = makeCompilerFixture();

    graph::Graph first{fixture.registry};
    const NodeId sourceA = add(first, "c.const");
    const NodeId shiftA = add(first, "c.shift");
    link(first, sourceA, shiftA, "source");

    // Dieselbe Kette, aber die Nummern sind andere (wie nach einem erneuten Einsetzen eines Looks im Live-Betrieb)
    graph::Graph second{fixture.registry};
    for (int i = 0; i < 5; ++i) {
        REQUIRE(second.removeNode(add(second, "c.const")).ok());
    }
    const NodeId shiftB = add(second, "c.shift");
    const NodeId sourceB = add(second, "c.const");
    link(second, sourceB, shiftB, "source");
    REQUIRE(sourceB.value() != sourceA.value());
    REQUIRE(shiftB.value() != shiftA.value());

    const auto a = compileField(first, fixture.library, out(shiftA));
    const auto b = compileField(second, fixture.library, out(shiftB));
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(a.value().wgsl == b.value().wgsl);
    CHECK(a.value().structureHash == b.value().structureHash);
    CHECK(a.value().wgsl.find('\x01') == std::string::npos);  // keine Platzhalter übrig
    CHECK(a.value().wgsl.find('\x02') == std::string::npos);

    // Die Namen folgen der Ausgabe: die Quelle zuerst, dann der Verbraucher
    const ParameterSlot* amount = slotFor(b.value().layout, shiftB, "amount");
    REQUIRE(amount != nullptr);
    CHECK(amount->member == "n2_amount");
    CHECK(b.value().wgsl.find("fn wh_f1(") < b.value().wgsl.find("fn wh_f2("));
    CHECK(b.value().wgsl.find("wh_params.n2_amount") != std::string::npos);
}

TEST_CASE("Shader-Namen: die Werte kommen weiter aus den richtigen Nodes", "[compiler][shader_names]") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    for (int i = 0; i < 3; ++i) {
        REQUIRE(graph.removeNode(add(graph, "c.const")).ok());
    }
    const NodeId shift = add(graph, "c.shift");
    const NodeId source = add(graph, "c.const");
    link(graph, source, shift, "source");
    REQUIRE(graph.setParameter(shift, "amount", 0.375).ok());

    const auto compiled = compileField(graph, fixture.library, out(shift));
    REQUIRE(compiled.ok());
    const ParameterSlot* amount = slotFor(compiled.value().layout, shift, "amount");
    REQUIRE(amount != nullptr);
    CHECK(amount->node == shift);  // die echte Nummer bleibt im Layout
    CHECK(floatAt(packParameters(graph, compiled.value().layout), amount->offsetBytes) == 0.375F);
}

TEST_CASE("Shader-Namen: eine andere Struktur ergibt einen anderen Schlüssel", "[compiler][shader_names]") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId source = add(graph, "c.const");
    const NodeId shift = add(graph, "c.shift");
    link(graph, source, shift, "source");
    const auto chained = compileField(graph, fixture.library, out(shift));
    const auto alone = compileField(graph, fixture.library, out(source));
    REQUIRE(chained.ok());
    REQUIRE(alone.ok());
    CHECK(chained.value().structureHash != alone.value().structureHash);
}
