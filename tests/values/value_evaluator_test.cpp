// Tests des ValueEvaluators: Reihenfolge nach Abhängigkeiten, Verbraucher sehen die Ergebnisse.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

#include "test_cpu_nodes.hpp"
#include "whitehole/graph/parameter_resolve.hpp"
#include "whitehole/values/value_evaluator.hpp"

using namespace whitehole;
using namespace whitehole::values;
using whitehole::test::makeValueFixture;
using whitehole::test::ValueFixture;

namespace {

struct Rig {
    ValueFixture fixture = makeValueFixture();
    graph::Graph graph{fixture.registry};
    ValueEvaluator evaluator{fixture.kernels};
    ValueStore store;
    std::uint64_t frame{0};

    NodeId add(std::string_view typeId) { return graph.addNode(typeId, Vec2{}).value(); }

    NodeId constant(double value) {
        const NodeId node = add("v.const");
        REQUIRE(graph.setParameter(node, "value", value).ok());
        return node;
    }

    void link(NodeId from, const std::string& fromPort, NodeId to, const std::string& toPort) {
        REQUIRE(graph.connect({from, fromPort}, {to, toPort}).ok());
    }

    Result<void> run(double seconds = 0.0, double delta = 0.0) {
        FrameTime time;
        time.frame = frame++;
        time.time = Seconds{seconds};
        time.delta = Seconds{delta};
        return evaluator.evaluate(graph, time, store);
    }

    double number(NodeId node, const std::string& port) const {
        const auto value = store.value(node, port);
        REQUIRE(value.has_value());
        return std::get<double>(*value);
    }
};

}  // namespace

TEST_CASE("Evaluator: Kette aus Konstanten und Addition, der Verbraucher sieht das Ergebnis") {
    Rig rig;
    const NodeId three = rig.constant(3.0);
    const NodeId four = rig.constant(4.0);
    const NodeId sum = rig.add("v.add");
    const NodeId target = rig.add("v.field");
    rig.link(three, "out", sum, "a");
    rig.link(four, "out", sum, "b");
    rig.link(sum, "out", target, "level");

    REQUIRE(rig.run().ok());

    CHECK(rig.number(three, "out") == 3.0);
    CHECK(rig.number(sum, "out") == 7.0);

    // Der Parameter des Verbrauchers nimmt den Wert der Quelle an.
    const graph::NodeInstance& node = *rig.graph.findNode(target);
    const graph::ParameterDescriptor& level = *rig.graph.registry().find("v.field")->findParameter("level");
    CHECK(std::get<double>(*graph::resolveParameter(rig.graph, node, level, &rig.store)) == 7.0);
}

TEST_CASE("Evaluator: Reihenfolge hängt nicht von der Anlege-Reihenfolge ab") {
    Rig rig;
    // Der Verbraucher wird zuerst angelegt, die Quellen danach.
    const NodeId target = rig.add("v.field");
    const NodeId sum = rig.add("v.add");
    const NodeId one = rig.constant(1.0);
    const NodeId two = rig.constant(2.0);
    rig.link(sum, "out", target, "level");
    rig.link(one, "out", sum, "a");
    rig.link(two, "out", sum, "b");

    REQUIRE(rig.run().ok());
    CHECK(rig.number(sum, "out") == 3.0);
}

TEST_CASE("Evaluator: unverbundene Eingänge nehmen den gespeicherten Parameterwert") {
    Rig rig;
    const NodeId sum = rig.add("v.add");
    const NodeId target = rig.add("v.field");
    REQUIRE(rig.graph.setParameter(sum, "a", 2.0).ok());
    REQUIRE(rig.graph.setParameter(sum, "b", 0.5).ok());
    rig.link(sum, "out", target, "level");

    REQUIRE(rig.run().ok());
    CHECK(rig.number(sum, "out") == 2.5);

    // Parameter ändern wirkt im nächsten Frame sofort.
    REQUIRE(rig.graph.setParameter(sum, "b", 1.5).ok());
    REQUIRE(rig.run().ok());
    CHECK(rig.number(sum, "out") == 3.5);
}

TEST_CASE("Evaluator: Eingang ohne Parameter liefert nur den verbundenen Wert") {
    Rig rig;
    const NodeId source = rig.constant(6.0);
    const NodeId pass = rig.add("v.pass");
    const NodeId lonely = rig.add("v.pass");
    const NodeId target = rig.add("v.field");
    const NodeId target2 = rig.add("v.field");
    rig.link(source, "out", pass, "in");
    rig.link(pass, "out", target, "level");
    rig.link(lonely, "out", target2, "level");

    REQUIRE(rig.run().ok());
    CHECK(rig.number(pass, "out") == 6.0);
    CHECK(rig.number(lonely, "out") == -1.0);  // Rückfallwert des Kernels
}

TEST_CASE("Evaluator: der Kernel sieht die Frame-Zeit") {
    Rig rig;
    const NodeId clock = rig.add("v.clock");
    const NodeId a = rig.add("v.field");
    const NodeId b = rig.add("v.field");
    rig.link(clock, "time", a, "level");
    rig.link(clock, "delta", b, "level");

    REQUIRE(rig.run(2.5, 0.5).ok());
    CHECK(rig.number(clock, "time") == 2.5);
    CHECK(rig.number(clock, "delta") == 0.5);
}

TEST_CASE("Evaluator: Nodes, die niemand braucht, laufen nicht") {
    Rig rig;

    SECTION("allein im Graphen") {
        const NodeId counter = rig.add("v.counter");
        REQUIRE(rig.run().ok());
        CHECK(rig.fixture.probes->counterRuns == 0);
        CHECK(rig.store.size() == 0U);
        CHECK_FALSE(rig.store.value(counter, "out").has_value());
        CHECK(rig.evaluator.instanceCount() == 0U);
    }
    SECTION("nur von einem Node gebraucht, der selbst keinen Verbraucher hat") {
        const NodeId counter = rig.add("v.counter");
        const NodeId sum = rig.add("v.add");
        rig.link(counter, "out", sum, "a");
        REQUIRE(rig.run().ok());
        CHECK(rig.fixture.probes->counterRuns == 0);
        CHECK(rig.store.size() == 0U);
    }
    SECTION("mit Verbraucher in der Field-Domäne läuft er") {
        const NodeId counter = rig.add("v.counter");
        const NodeId target = rig.add("v.field");
        rig.link(counter, "out", target, "level");
        REQUIRE(rig.run().ok());
        CHECK(rig.fixture.probes->counterRuns == 1);
    }
    SECTION("transitiv: Kette bis zu einem Field-Node") {
        const NodeId counter = rig.add("v.counter");
        const NodeId sum = rig.add("v.add");
        const NodeId target = rig.add("v.field");
        rig.link(counter, "out", sum, "a");
        rig.link(sum, "out", target, "level");
        REQUIRE(rig.run().ok());
        CHECK(rig.fixture.probes->counterRuns == 1);
        CHECK(rig.number(sum, "out") == 1.0);
    }
}

TEST_CASE("Evaluator: Senken (CPU-Nodes ohne Ausgänge) laufen immer und sehen ihre Vorgänger") {
    Rig rig;
    const NodeId source = rig.constant(42.0);
    const NodeId sink = rig.add("v.sink");
    rig.link(source, "out", sink, "value");

    REQUIRE(rig.run().ok());
    REQUIRE(rig.fixture.probes->sinkSeen.has_value());
    CHECK(*rig.fixture.probes->sinkSeen == 42.0);
    CHECK(rig.number(source, "out") == 42.0);  // die Quelle wird mitgezogen
}

TEST_CASE("Evaluator: der Kernel behält seinen Zustand über Frames hinweg") {
    Rig rig;
    const NodeId counter = rig.add("v.counter");
    const NodeId target = rig.add("v.field");
    rig.link(counter, "out", target, "level");

    for (int expected = 1; expected <= 4; ++expected) {
        REQUIRE(rig.run().ok());
        CHECK(rig.number(counter, "out") == static_cast<double>(expected));
    }
    CHECK(rig.evaluator.instanceCount() == 1U);
    CHECK(rig.fixture.probes->kernelsCreated == 1);  // nur ein einziges Mal erzeugt
}

TEST_CASE("Evaluator: Instanzen gelöschter Nodes werden verworfen, neue Nodes starten frisch") {
    Rig rig;
    const NodeId first = rig.add("v.counter");
    const NodeId target = rig.add("v.field");
    rig.link(first, "out", target, "level");
    REQUIRE(rig.run().ok());
    REQUIRE(rig.run().ok());
    CHECK(rig.number(first, "out") == 2.0);

    REQUIRE(rig.graph.removeNode(first).ok());
    REQUIRE(rig.run().ok());
    CHECK(rig.evaluator.instanceCount() == 0U);

    const NodeId second = rig.add("v.counter");
    rig.link(second, "out", target, "level");
    REQUIRE(rig.run().ok());
    CHECK(rig.number(second, "out") == 1.0);
    CHECK(rig.fixture.probes->kernelsCreated == 2);
}

TEST_CASE("Evaluator: jeder Frame beginnt mit leerem Speicher") {
    Rig rig;
    const NodeId source = rig.constant(1.0);
    const NodeId target = rig.add("v.field");
    rig.link(source, "out", target, "level");
    REQUIRE(rig.run().ok());
    REQUIRE(rig.store.size() == 1U);

    const graph::Connection* connection = rig.graph.findInputConnection(target, "level");
    REQUIRE(connection != nullptr);
    REQUIRE(rig.graph.disconnect(connection->id).ok());
    REQUIRE(rig.run().ok());
    CHECK(rig.store.size() == 0U);
}

TEST_CASE("Evaluator: gleicher Graph und gleiche Zeit liefern gleiche Werte") {
    Rig a;
    Rig b;
    for (Rig* rig : {&a, &b}) {
        const NodeId clock = rig->add("v.clock");
        const NodeId sum = rig->add("v.add");
        const NodeId target = rig->add("v.field");
        rig->link(clock, "time", sum, "a");
        rig->link(clock, "delta", sum, "b");
        rig->link(sum, "out", target, "level");
        REQUIRE(rig->run(1.25, 0.25).ok());
    }
    CHECK(a.number(NodeId{2}, "out") == b.number(NodeId{2}, "out"));
    CHECK(a.number(NodeId{2}, "out") == 1.5);
}

TEST_CASE("Evaluator: Fehler werden gemeldet und beschädigen frühere Ergebnisse nicht") {
    Rig rig;
    const NodeId good = rig.constant(5.0);
    const NodeId target = rig.add("v.field");
    rig.link(good, "out", target, "level");

    SECTION("Typ ohne Kernel") {
        const NodeId broken = rig.add("v.nokernel");
        const NodeId second = rig.add("v.field");
        rig.link(broken, "out", second, "level");
        const Result<void> result = rig.run();
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().code == ErrorCode::NotFound);
        CHECK(result.error().message.find("v.nokernel") != std::string::npos);
    }
    SECTION("Kernel liefert den falschen Typ") {
        const NodeId broken = rig.add("v.badtype");
        const NodeId second = rig.add("v.field");
        rig.link(broken, "out", second, "level");
        const Result<void> result = rig.run();
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().code == ErrorCode::InvalidArgument);
        CHECK_FALSE(rig.store.value(broken, "out").has_value());  // der falsche Wert gelangt nicht in den Speicher
    }
    SECTION("Kernel liefert einen Ausgang, den es nicht gibt") {
        const NodeId broken = rig.add("v.ghost");
        const NodeId second = rig.add("v.field");
        rig.link(broken, "out", second, "level");
        const Result<void> result = rig.run();
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().code == ErrorCode::InvalidArgument);
        CHECK(result.error().message.find("nope") != std::string::npos);
    }
}
