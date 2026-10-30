// Werte-Nodes im Zusammenspiel mit dem Feldcompiler: vom Evaluator über den Wertespeicher
// bis in den Parameterpuffer, der auf die GPU geht.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "builtin_lab.hpp"

using namespace whitehole;
using whitehole::test::floatAt;
using whitehole::test::Lab;
using whitehole::test::slotFor;

TEST_CASE("Durchstich: Time steuert Rotate.angle, nur der Parameterpuffer ändert sich") {
    Lab lab;
    const NodeId gradient = lab.add("wh.generator.gradient");
    const NodeId rotate = lab.add("wh.transform.rotate");
    const NodeId time = lab.add("wh.value.time");
    lab.connect(gradient, "out", rotate, "source");

    const graph::PortRef target{rotate, "out"};
    const auto before = compiler::compileField(lab.graph, lab.snippets, target);
    REQUIRE(before.ok());

    lab.connect(time, "seconds", rotate, "angle");
    const auto compiled = compiler::compileField(lab.graph, lab.snippets, target);
    REQUIRE(compiled.ok());
    CHECK(compiled.value().structureHash == before.value().structureHash);  // Verbinden kostet keine Neukompilierung

    const compiler::ParameterSlot* angle = slotFor(compiled.value().layout, rotate, "angle");
    REQUIRE(angle != nullptr);

    lab.run(0.0);
    CHECK(floatAt(compiler::packParameters(lab.graph, compiled.value().layout, &lab.store), angle->offsetBytes) == 0.0F);
    lab.run(10.0);
    CHECK(floatAt(compiler::packParameters(lab.graph, compiled.value().layout, &lab.store), angle->offsetBytes) == 10.0F);
    lab.run(37.5);
    CHECK(floatAt(compiler::packParameters(lab.graph, compiled.value().layout, &lab.store), angle->offsetBytes) == 37.5F);

    // Ohne Werte gilt wieder der gespeicherte Winkel.
    CHECK(floatAt(compiler::packParameters(lab.graph, compiled.value().layout), angle->offsetBytes) == 0.0F);
}

TEST_CASE("Durchstich: Color steuert eine Farbe, Number liefert ein konstantes Feld") {
    Lab lab;
    const NodeId gradient = lab.add("wh.generator.gradient");
    const NodeId color = lab.add("wh.value.color");
    const NodeId number = lab.add("wh.value.number");
    const NodeId plasma = lab.add("wh.generator.plasma");
    const NodeId mix = lab.add("wh.blend.mix");

    lab.connect(color, "out", gradient, "colorA");
    lab.connect(gradient, "out", mix, "a");
    lab.connect(plasma, "out", mix, "b");
    lab.set(color, "color", ColorRgba{0.9F, 0.8F, 0.7F, 1.0F});

    SECTION("Farbe moduliert colorA") {
        const auto compiled = compiler::compileField(lab.graph, lab.snippets, graph::PortRef{mix, "out"});
        REQUIRE(compiled.ok());
        const compiler::ParameterSlot* slot = slotFor(compiled.value().layout, gradient, "colorA");
        REQUIRE(slot != nullptr);

        lab.run(0.0);
        const auto bytes = compiler::packParameters(lab.graph, compiled.value().layout, &lab.store);
        CHECK(floatAt(bytes, slot->offsetBytes) == 0.9F);
        CHECK(floatAt(bytes, slot->offsetBytes + 4U) == 0.8F);
        CHECK(floatAt(bytes, slot->offsetBytes + 8U) == 0.7F);
    }
    SECTION("Zahl an einem Field-Eingang wird ein konstantes Feld") {
        const NodeId add = lab.add("wh.blend.add");
        lab.connect(number, "out", add, "a");
        lab.connect(plasma, "out", add, "b");
        lab.set(number, "value", 0.5);

        const auto compiled = compiler::compileField(lab.graph, lab.snippets, graph::PortRef{add, "out"});
        REQUIRE(compiled.ok());
        CHECK(compiled.value().nodes.size() == 2U);  // Add und Plasma; Number ist kein Shader-Code
        const compiler::ParameterSlot* slot = slotFor(compiled.value().layout, add, "a");
        REQUIRE(slot != nullptr);
        CHECK(slot->origin == compiler::SlotOrigin::FieldConstant);

        lab.run(0.0);
        CHECK(floatAt(compiler::packParameters(lab.graph, compiled.value().layout, &lab.store), slot->offsetBytes) == 0.5F);
    }
}
