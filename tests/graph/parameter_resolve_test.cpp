// Tests der Modulation: welcher Parameter über welchen Port gesteuert wird und welcher Wert zu welchem Porttyp passt.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "fake_value_lookup.hpp"
#include "test_nodes.hpp"
#include "whitehole/graph/parameter_resolve.hpp"

using namespace whitehole;
using namespace whitehole::graph;
using whitehole::test::FakeValueLookup;
using whitehole::test::inPort;
using whitehole::test::mustRegister;
using whitehole::test::outPort;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

ParameterDescriptor parameter(const std::string& key, ParameterValue value,
                              std::optional<double> minValue = std::nullopt,
                              std::optional<double> maxValue = std::nullopt) {
    return ParameterDescriptor{key, key, std::move(value), minValue, maxValue};
}

/// pr.target: modulierbare Parameter gain/count/on/tint, dazu Parameter ohne Eingang.
/// pr.src:    ein Ausgang je Werttyp.
std::shared_ptr<NodeRegistry> makeRegistry() {
    auto registry = std::make_shared<NodeRegistry>();

    NodeDescriptor target;
    target.typeId = "pr.target";
    target.ports = {inPort("gain", PortType::Scalar), inPort("count", PortType::Integer),
                    inPort("on", PortType::Boolean), inPort("tint", PortType::Color)};
    target.parameters = {parameter("gain", 0.5, 0.0, 1.0),
                         parameter("count", std::int64_t{1}, 0.0, 10.0),
                         parameter("big", std::int64_t{5}),
                         parameter("on", false),
                         parameter("tint", ColorRgba{1.0F, 1.0F, 1.0F, 1.0F}),
                         parameter("name", std::string{"x"}),
                         parameter("offset", Vec2{})};
    mustRegister(*registry, target);

    NodeDescriptor source;
    source.typeId = "pr.src";
    source.ports = {outPort("num", PortType::Scalar),   outPort("int", PortType::Integer),
                    outPort("flag", PortType::Boolean), outPort("col", PortType::Color),
                    outPort("trig", PortType::Trigger)};
    mustRegister(*registry, source);
    return registry;
}

const ParameterDescriptor& param(const Graph& graph, const char* key) {
    return *graph.registry().find("pr.target")->findParameter(key);
}

}  // namespace

TEST_CASE("modulationPortType: welcher Parameter wird über welchen Port moduliert") {
    CHECK(modulationPortType(ParameterValue{1.5}) == PortType::Scalar);
    CHECK(modulationPortType(ParameterValue{std::int64_t{1}}) == PortType::Integer);
    CHECK(modulationPortType(ParameterValue{true}) == PortType::Boolean);
    CHECK(modulationPortType(ParameterValue{ColorRgba{}}) == PortType::Color);
    CHECK_FALSE(modulationPortType(ParameterValue{Vec2{}}).has_value());
    CHECK_FALSE(modulationPortType(ParameterValue{Vec3{}}).has_value());
    CHECK_FALSE(modulationPortType(ParameterValue{std::string{"x"}}).has_value());
}

TEST_CASE("valueMatchesPortType") {
    CHECK(valueMatchesPortType(PortType::Scalar, ParameterValue{1.0}));
    CHECK_FALSE(valueMatchesPortType(PortType::Scalar, ParameterValue{std::int64_t{1}}));
    CHECK(valueMatchesPortType(PortType::Integer, ParameterValue{std::int64_t{1}}));
    CHECK(valueMatchesPortType(PortType::Boolean, ParameterValue{true}));
    CHECK(valueMatchesPortType(PortType::Trigger, ParameterValue{true}));
    CHECK_FALSE(valueMatchesPortType(PortType::Trigger, ParameterValue{1.0}));
    CHECK(valueMatchesPortType(PortType::Color, ParameterValue{ColorRgba{}}));
    CHECK(valueMatchesPortType(PortType::Text, ParameterValue{std::string{"x"}}));
    CHECK_FALSE(valueMatchesPortType(PortType::Image, ParameterValue{1.0}));
    CHECK_FALSE(valueMatchesPortType(PortType::Field, ParameterValue{1.0}));
}

TEST_CASE("addModulationPorts: ein Eingang je modulierbarem Parameter, nie doppelt") {
    NodeDescriptor descriptor;
    descriptor.ports = {outPort("out", PortType::Scalar)};
    descriptor.parameters = {ParameterDescriptor{"speed", "Speed (Hz)", 1.0, std::nullopt, std::nullopt},
                             parameter("mode", std::int64_t{0}), parameter("on", true),
                             parameter("color", ColorRgba{}), parameter("offset", Vec2{}),
                             parameter("label", std::string{"x"})};

    addModulationPorts(descriptor);

    REQUIRE(descriptor.ports.size() == 5U);  // out + speed, mode, on, color
    const PortDescriptor* speed = descriptor.findPort("speed");
    REQUIRE(speed != nullptr);
    CHECK(speed->type == PortType::Scalar);
    CHECK(speed->direction == PortDirection::Input);
    CHECK_FALSE(speed->required);
    CHECK(speed->displayName == "Speed (Hz)");
    CHECK(descriptor.findPort("mode")->type == PortType::Integer);
    CHECK(descriptor.findPort("on")->type == PortType::Boolean);
    CHECK(descriptor.findPort("color")->type == PortType::Color);
    CHECK(descriptor.findPort("offset") == nullptr);
    CHECK(descriptor.findPort("label") == nullptr);

    addModulationPorts(descriptor);  // idempotent
    CHECK(descriptor.ports.size() == 5U);
}

TEST_CASE("convertForParameter: Zahlen") {
    const auto registry = makeRegistry();
    const Graph graph{registry};
    const ParameterDescriptor& gain = param(graph, "gain");  // 0..1

    SECTION("double wird begrenzt") {
        CHECK(std::get<double>(*convertForParameter(ParameterValue{0.25}, gain)) == 0.25);
        CHECK(std::get<double>(*convertForParameter(ParameterValue{9.0}, gain)) == 1.0);
        CHECK(std::get<double>(*convertForParameter(ParameterValue{-9.0}, gain)) == 0.0);
    }
    SECTION("int64 und bool lassen sich als Zahl lesen") {
        CHECK(std::get<double>(*convertForParameter(ParameterValue{std::int64_t{1}}, gain)) == 1.0);
        CHECK(std::get<double>(*convertForParameter(ParameterValue{true}, gain)) == 1.0);
        CHECK(std::get<double>(*convertForParameter(ParameterValue{false}, gain)) == 0.0);
    }
    SECTION("nicht endliche Zahlen und andere Typen werden abgelehnt") {
        CHECK_FALSE(convertForParameter(ParameterValue{kNaN}, gain).has_value());
        CHECK_FALSE(convertForParameter(ParameterValue{kInf}, gain).has_value());
        CHECK_FALSE(convertForParameter(ParameterValue{-kInf}, gain).has_value());
        CHECK_FALSE(convertForParameter(ParameterValue{ColorRgba{}}, gain).has_value());
        CHECK_FALSE(convertForParameter(ParameterValue{std::string{"1"}}, gain).has_value());
    }
}

TEST_CASE("convertForParameter: Ganzzahl, Bool, Farbe und Typen ohne Modulation") {
    const auto registry = makeRegistry();
    const Graph graph{registry};

    SECTION("int64 mit Bereich wird begrenzt, double wird abgelehnt") {
        const ParameterDescriptor& count = param(graph, "count");  // 0..10
        CHECK(std::get<std::int64_t>(*convertForParameter(ParameterValue{std::int64_t{4}}, count)) == 4);
        CHECK(std::get<std::int64_t>(*convertForParameter(ParameterValue{std::int64_t{99}}, count)) == 10);
        CHECK(std::get<std::int64_t>(*convertForParameter(ParameterValue{std::int64_t{-5}}, count)) == 0);
        CHECK_FALSE(convertForParameter(ParameterValue{2.0}, count).has_value());
    }
    SECTION("int64 ohne Grenzen bleibt auch über 2^53 exakt") {
        const std::int64_t huge = (std::int64_t{1} << 60) + 1;
        const auto converted = convertForParameter(ParameterValue{huge}, param(graph, "big"));
        REQUIRE(converted.has_value());
        CHECK(std::get<std::int64_t>(*converted) == huge);
    }
    SECTION("bool nimmt nur bool") {
        const ParameterDescriptor& on = param(graph, "on");
        CHECK(std::get<bool>(*convertForParameter(ParameterValue{true}, on)));
        CHECK_FALSE(convertForParameter(ParameterValue{1.0}, on).has_value());
        CHECK_FALSE(convertForParameter(ParameterValue{std::int64_t{1}}, on).has_value());
    }
    SECTION("Farbe nimmt nur endliche Farben") {
        const ParameterDescriptor& tint = param(graph, "tint");
        const ColorRgba color{0.1F, 0.2F, 0.3F, 0.4F};
        CHECK(std::get<ColorRgba>(*convertForParameter(ParameterValue{color}, tint)) == color);
        ColorRgba broken = color;
        broken.g = std::numeric_limits<float>::quiet_NaN();
        CHECK_FALSE(convertForParameter(ParameterValue{broken}, tint).has_value());
        CHECK_FALSE(convertForParameter(ParameterValue{1.0}, tint).has_value());
    }
    SECTION("Vec2 und Text lassen sich nicht modulieren") {
        CHECK_FALSE(convertForParameter(ParameterValue{Vec2{}}, param(graph, "offset")).has_value());
        CHECK_FALSE(convertForParameter(ParameterValue{std::string{"x"}}, param(graph, "name")).has_value());
    }
}

TEST_CASE("resolveParameter: gespeicherter Wert oder Wert der verbundenen Quelle") {
    const auto registry = makeRegistry();
    Graph graph{registry};
    const NodeId target = graph.addNode("pr.target", Vec2{}).value();
    const NodeId source = graph.addNode("pr.src", Vec2{}).value();
    const NodeInstance& node = *graph.findNode(target);
    const ParameterDescriptor& gain = param(graph, "gain");

    FakeValueLookup lookup;
    lookup.set(source, "num", 0.8);

    SECTION("unverbunden gilt der gespeicherte Wert, auch mit Lookup") {
        CHECK(std::get<double>(*resolveParameter(graph, node, gain, &lookup)) == 0.5);
        CHECK(std::get<double>(*resolveParameter(graph, node, gain, nullptr)) == 0.5);
    }
    SECTION("verbunden und geliefert") {
        REQUIRE(graph.connect({source, "num"}, {target, "gain"}).ok());
        CHECK(std::get<double>(*resolveParameter(graph, node, gain, &lookup)) == 0.8);
        CHECK(std::get<double>(*resolveParameter(graph, node, gain, nullptr)) == 0.5);
    }
    SECTION("verbunden, aber die Quelle hat nichts geliefert") {
        REQUIRE(graph.connect({source, "num"}, {target, "gain"}).ok());
        lookup.clear();
        CHECK(std::get<double>(*resolveParameter(graph, node, gain, &lookup)) == 0.5);
    }
    SECTION("verbunden, aber der gelieferte Wert ist unbrauchbar") {
        REQUIRE(graph.connect({source, "num"}, {target, "gain"}).ok());
        lookup.set(source, "num", kNaN);
        CHECK(std::get<double>(*resolveParameter(graph, node, gain, &lookup)) == 0.5);
    }
    SECTION("der geänderte gespeicherte Wert wird gelesen") {
        REQUIRE(graph.setParameter(target, "gain", 0.125).ok());
        CHECK(std::get<double>(*resolveParameter(graph, *graph.findNode(target), gain, &lookup)) == 0.125);
    }
    SECTION("Integer und Boolean dürfen eine Zahl steuern") {
        REQUIRE(graph.connect({source, "int"}, {target, "gain"}).ok());
        lookup.set(source, "int", std::int64_t{1});
        CHECK(std::get<double>(*resolveParameter(graph, node, gain, &lookup)) == 1.0);
    }
    SECTION("Trigger steuert einen Boolean") {
        REQUIRE(graph.connect({source, "trig"}, {target, "on"}).ok());
        lookup.set(source, "trig", true);
        CHECK(std::get<bool>(*resolveParameter(graph, node, param(graph, "on"), &lookup)));
    }
    SECTION("Farbe") {
        REQUIRE(graph.connect({source, "col"}, {target, "tint"}).ok());
        lookup.set(source, "col", ColorRgba{0.0F, 1.0F, 0.0F, 1.0F});
        CHECK(std::get<ColorRgba>(*resolveParameter(graph, node, param(graph, "tint"), &lookup)).g == 1.0F);
    }
}
