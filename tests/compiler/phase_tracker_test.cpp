// Tests des Phase-Verfolgers und der Phase-Slots in packParameters: Eine Animation läuft beim Verstellen eines Reglers
// stetig weiter (statt zu springen), und bei unberührtem Regler bleibt alles bitgleich zu `Zeit * Regler`.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

#include "test_field_nodes.hpp"
#include "whitehole/compiler/parameter_layout.hpp"
#include "whitehole/compiler/phase_tracker.hpp"

using namespace whitehole;
using namespace whitehole::compiler;
using whitehole::test::makeCompilerFixture;

namespace {

float floatAt(const std::vector<std::byte>& bytes, std::uint32_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

}  // namespace

TEST_CASE("PhaseTracker: unberührte Rate ergibt bitgleich Zeit mal Rate") {
    PhaseTracker tracker;
    const NodeId node{3};
    for (const double time : {0.0, 0.016, 1.0 / 3.0, 12.5, 600.25, 86400.1}) {
        CHECK(tracker.phase(node, "speed", time, 3.7) == time * 3.7);
    }
    CHECK(tracker.size() == 1U);
}

TEST_CASE("PhaseTracker: eine Änderung der Rate lässt die Phase im Augenblick der Änderung gleich") {
    PhaseTracker tracker;
    const NodeId node{1};
    CHECK(tracker.phase(node, "speed", 10.0, 2.0) == 20.0);

    // Regler auf 5: bei t = 10 bleibt es bei 20, danach läuft die Phase mit der neuen Rate weiter.
    CHECK(tracker.phase(node, "speed", 10.0, 5.0) == 20.0);
    CHECK(tracker.phase(node, "speed", 12.0, 5.0) == 30.0);
    CHECK(tracker.phase(node, "speed", 14.0, 5.0) == 40.0);

    // Und noch einmal: auf 0 gestellt, steht die Animation.
    CHECK(tracker.phase(node, "speed", 14.0, 0.0) == 40.0);
    CHECK(tracker.phase(node, "speed", 20.0, 0.0) == 40.0);
}

TEST_CASE("PhaseTracker: Springen der Zeit ist unkritisch, die Phase ist eine reine Funktion der Zeit") {
    PhaseTracker tracker;
    const NodeId node{1};
    (void)tracker.phase(node, "speed", 10.0, 2.0);
    (void)tracker.phase(node, "speed", 10.0, 5.0);  // ab hier Rate 5, Versatz -30

    const double at12 = tracker.phase(node, "speed", 12.0, 5.0);
    CHECK(tracker.phase(node, "speed", 3.0, 5.0) == -15.0);  // zurück gescrubbt
    CHECK(tracker.phase(node, "speed", 12.0, 5.0) == at12);   // und wieder vor: dasselbe wie vorher
    CHECK(tracker.phase(node, "speed", 0.0, 5.0) == -30.0);
}

TEST_CASE("PhaseTracker: eine über die Zeit veränderte Rate ergibt das Integral") {
    PhaseTracker tracker;
    const NodeId node{2};
    // 60 Bilder pro Sekunde, die Rate steigt jedes Bild um 0,1. Erwartet: Summe rate(i-1) * dt (Euler vorwärts).
    constexpr double dt = 1.0 / 64.0;  // exakt darstellbar, damit die Summe sauber bleibt
    double expected = 0.0;
    double previousRate = 0.0;
    for (int i = 0; i < 640; ++i) {
        const double time = i * dt;
        const double rate = 1.0 + 0.1 * i;
        if (i > 0) {
            expected += dt * previousRate;
        }
        const double got = tracker.phase(node, "speed", time, rate);
        CHECK_THAT(got, Catch::Matchers::WithinAbs(expected, 1e-9));
        previousRate = rate;
    }
}

TEST_CASE("PhaseTracker: Nodes und Schlüssel sind unabhängig") {
    PhaseTracker tracker;
    (void)tracker.phase(NodeId{1}, "speed", 10.0, 2.0);
    (void)tracker.phase(NodeId{1}, "drift", 10.0, 1.0);
    (void)tracker.phase(NodeId{2}, "speed", 10.0, 4.0);
    CHECK(tracker.size() == 3U);

    CHECK(tracker.phase(NodeId{1}, "speed", 10.0, 6.0) == 20.0);  // nur dieser Eintrag bekommt einen Versatz
    CHECK(tracker.phase(NodeId{1}, "drift", 12.0, 1.0) == 12.0);
    CHECK(tracker.phase(NodeId{2}, "speed", 12.0, 4.0) == 48.0);
}

TEST_CASE("PhaseTracker: eine nicht endliche Rate legt nichts an und ändert nichts") {
    PhaseTracker tracker;
    const NodeId node{1};
    (void)tracker.phase(node, "speed", 1.0, 2.0);
    CHECK(std::isnan(tracker.phase(node, "speed", 5.0, std::numeric_limits<double>::quiet_NaN())));
    CHECK(tracker.phase(node, "speed", 5.0, 2.0) == 10.0);  // der Eintrag ist unverändert
}

TEST_CASE("PhaseTracker::retain vergisst Einträge, die das Layout nicht mehr braucht") {
    PhaseTracker tracker;
    (void)tracker.phase(NodeId{1}, "speed", 1.0, 2.0);
    (void)tracker.phase(NodeId{2}, "speed", 1.0, 2.0);
    (void)tracker.phase(NodeId{1}, "other", 1.0, 2.0);

    LayoutBuilder builder;
    builder.add(NodeId{1}, "speed", GpuType::Float);                                 // Parameter-Slot zählt nicht
    builder.add(NodeId{1}, phaseSlotKey("speed"), GpuType::Float, SlotOrigin::Phase);  // nur dieser bleibt
    tracker.retain(builder.build());
    CHECK(tracker.size() == 1U);

    tracker.retain(LayoutBuilder{}.build());
    CHECK(tracker.size() == 0U);
}

TEST_CASE("phaseSlotKey und parameterKeyOfPhaseSlot sind Umkehrungen") {
    CHECK(phaseSlotKey("speed") == "phase_speed");
    CHECK(parameterKeyOfPhaseSlot(phaseSlotKey("speed")) == "speed");
    CHECK(parameterKeyOfPhaseSlot("rate") == "rate");  // ohne Vorsatz unverändert
}

TEST_CASE("packParameters: Phase-Slot bekommt die Phase der aktuellen Rate des Parameters") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = graph.addNode("c.const", Vec2{}).value();  // Parameter "level" (Float, 0..1)

    LayoutBuilder builder;
    builder.add(node, phaseSlotKey("level"), GpuType::Float, SlotOrigin::Phase);
    const ParameterLayout layout = builder.build();
    const std::uint32_t at = layout.slots[0].offsetBytes;

    PhaseTracker tracker;
    REQUIRE(graph.setParameter(node, "level", 0.5).ok());
    CHECK(floatAt(packParameters(graph, layout, nullptr, PhaseSource{&tracker, 10.0}), at) == 5.0F);
    CHECK(floatAt(packParameters(graph, layout, nullptr, PhaseSource{&tracker, 12.0}), at) == 6.0F);

    // Regler verstellt: bei t = 12 bleibt die Phase bei 6, danach läuft sie mit der neuen Rate (1,0) weiter.
    REQUIRE(graph.setParameter(node, "level", 1.0).ok());
    CHECK(floatAt(packParameters(graph, layout, nullptr, PhaseSource{&tracker, 12.0}), at) == 6.0F);
    CHECK(floatAt(packParameters(graph, layout, nullptr, PhaseSource{&tracker, 14.0}), at) == 8.0F);
}

TEST_CASE("packParameters: ohne Verfolger ist die Phase Zeit mal Rate (wie @time * @param)") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = graph.addNode("c.const", Vec2{}).value();
    REQUIRE(graph.setParameter(node, "level", 0.5).ok());

    LayoutBuilder builder;
    builder.add(node, phaseSlotKey("level"), GpuType::Float, SlotOrigin::Phase);
    const ParameterLayout layout = builder.build();

    CHECK(floatAt(packParameters(graph, layout, nullptr, PhaseSource{nullptr, 8.0}), layout.slots[0].offsetBytes) == 4.0F);
    CHECK(floatAt(packParameters(graph, layout), layout.slots[0].offsetBytes) == 0.0F);  // Zeit 0
}

TEST_CASE("packParameters: Phase-Slot zu fehlendem oder nicht passendem Parameter bleibt null") {
    const auto fixture = makeCompilerFixture();
    graph::Graph graph{fixture.registry};
    const NodeId node = graph.addNode("c.const", Vec2{}).value();

    LayoutBuilder builder;
    builder.add(node, phaseSlotKey("gibtEsNicht"), GpuType::Float, SlotOrigin::Phase);
    builder.add(node, phaseSlotKey("color"), GpuType::Float, SlotOrigin::Phase);  // Farbe hat keine Rate
    const ParameterLayout layout = builder.build();

    PhaseTracker tracker;
    const std::vector<std::byte> bytes = packParameters(graph, layout, nullptr, PhaseSource{&tracker, 5.0});
    CHECK(floatAt(bytes, layout.slots[0].offsetBytes) == 0.0F);
    CHECK(floatAt(bytes, layout.slots[1].offsetBytes) == 0.0F);
    CHECK(tracker.size() == 0U);
}
