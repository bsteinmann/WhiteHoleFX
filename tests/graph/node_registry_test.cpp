#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "test_nodes.hpp"
#include "whitehole/graph/node_registry_impl.hpp"

using namespace whitehole;
using namespace whitehole::graph;
using whitehole::test::outPort;

namespace {

NodeDescriptor minimal(const std::string& typeId) {
    NodeDescriptor descriptor;
    descriptor.typeId = typeId;
    descriptor.displayName = typeId;
    descriptor.ports = {outPort("out", PortType::Field)};
    return descriptor;
}

}  // namespace

TEST_CASE("NodeRegistry: Eintragen und Finden") {
    NodeRegistry registry;
    CHECK(registry.find("a.b") == nullptr);

    REQUIRE(registry.registerNode(minimal("a.b")).ok());

    const NodeDescriptor* found = registry.find("a.b");
    REQUIRE(found != nullptr);
    CHECK(found->typeId == "a.b");
}

TEST_CASE("NodeRegistry: doppelte Typ-ID wird abgelehnt") {
    NodeRegistry registry;
    REQUIRE(registry.registerNode(minimal("a.b")).ok());

    const Result<void> second = registry.registerNode(minimal("a.b"));
    REQUIRE_FALSE(second.ok());
    CHECK(second.error().code == ErrorCode::AlreadyExists);
}

TEST_CASE("NodeRegistry: leere Typ-ID wird abgelehnt") {
    NodeRegistry registry;
    const Result<void> result = registry.registerNode(minimal(""));
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("NodeRegistry: ungültige Schlüssel werden abgelehnt und nichts wird aufgenommen") {
    NodeRegistry registry;

    for (const char* badKey : {"", "1bad", "has space", "dash-ed", "ümlaut"}) {
        NodeDescriptor descriptor = minimal("bad.port");
        descriptor.ports.push_back(outPort(badKey, PortType::Field));
        const Result<void> result = registry.registerNode(descriptor);
        CHECK_FALSE(result.ok());
        CHECK(registry.find("bad.port") == nullptr);
    }

    NodeDescriptor badParameter = minimal("bad.param");
    badParameter.parameters = {ParameterDescriptor{"not valid", "X", 1.0, std::nullopt, std::nullopt}};
    CHECK_FALSE(registry.registerNode(badParameter).ok());

    // Gültige Sonderfälle
    NodeDescriptor fine = minimal("fine");
    fine.ports.push_back(outPort("_under_score9", PortType::Field));
    CHECK(registry.registerNode(fine).ok());
}

TEST_CASE("NodeRegistry: doppelte Port- und Parameterschlüssel sowie min > max werden abgelehnt") {
    NodeRegistry registry;

    NodeDescriptor duplicatePort = minimal("dup.port");
    duplicatePort.ports.push_back(outPort("out", PortType::Field));
    CHECK_FALSE(registry.registerNode(duplicatePort).ok());

    NodeDescriptor duplicateParameter = minimal("dup.param");
    duplicateParameter.parameters = {
        ParameterDescriptor{"speed", "Speed", 1.0, std::nullopt, std::nullopt},
        ParameterDescriptor{"speed", "Speed 2", 2.0, std::nullopt, std::nullopt}};
    CHECK_FALSE(registry.registerNode(duplicateParameter).ok());

    NodeDescriptor reversedRange = minimal("range");
    reversedRange.parameters = {ParameterDescriptor{"amount", "Amount", 0.5, 1.0, 0.0}};
    const Result<void> result = registry.registerNode(reversedRange);
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("NodeRegistry: Modulations-Eingänge müssen zum Typ ihres Parameters passen") {
    using whitehole::test::inPort;
    NodeRegistry registry;

    SECTION("passende Paare werden akzeptiert") {
        NodeDescriptor descriptor = minimal("mod.ok");
        descriptor.ports.push_back(inPort("gain", PortType::Scalar));
        descriptor.ports.push_back(inPort("count", PortType::Integer));
        descriptor.ports.push_back(inPort("on", PortType::Boolean));
        descriptor.ports.push_back(inPort("tint", PortType::Color));
        descriptor.parameters = {ParameterDescriptor{"gain", "Gain", 0.5, std::nullopt, std::nullopt},
                                 ParameterDescriptor{"count", "Count", std::int64_t{1}, std::nullopt, std::nullopt},
                                 ParameterDescriptor{"on", "On", true, std::nullopt, std::nullopt},
                                 ParameterDescriptor{"tint", "Tint", ColorRgba{}, std::nullopt, std::nullopt}};
        CHECK(registry.registerNode(descriptor).ok());
    }

    SECTION("falscher Portyp") {
        NodeDescriptor descriptor = minimal("mod.type");
        descriptor.ports.push_back(inPort("gain", PortType::Integer));
        descriptor.parameters = {ParameterDescriptor{"gain", "Gain", 0.5, std::nullopt, std::nullopt}};
        const Result<void> result = registry.registerNode(descriptor);
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().code == ErrorCode::InvalidArgument);
        CHECK(registry.find("mod.type") == nullptr);  // nichts wurde aufgenommen
    }

    SECTION("Field-Eingang mit dem Schlüssel eines Parameters") {
        NodeDescriptor descriptor = minimal("mod.field");
        descriptor.ports.push_back(inPort("amount", PortType::Field));
        descriptor.parameters = {ParameterDescriptor{"amount", "Amount", 0.5, std::nullopt, std::nullopt}};
        CHECK_FALSE(registry.registerNode(descriptor).ok());
    }

    SECTION("Parameter ohne Portyp (Vec2) kann keinen Eingang gleichen Schlüssels haben") {
        NodeDescriptor descriptor = minimal("mod.vec");
        descriptor.ports.push_back(inPort("offset", PortType::Scalar));
        descriptor.parameters = {ParameterDescriptor{"offset", "Offset", Vec2{}, std::nullopt, std::nullopt}};
        CHECK_FALSE(registry.registerNode(descriptor).ok());
    }

    SECTION("ein Ausgang darf den Schlüssel eines Parameters tragen") {
        NodeDescriptor descriptor = minimal("mod.output");
        descriptor.ports.push_back(outPort("value", PortType::Scalar));
        descriptor.parameters = {ParameterDescriptor{"value", "Value", 0.0, std::nullopt, std::nullopt}};
        CHECK(registry.registerNode(descriptor).ok());
    }
}

TEST_CASE("NodeRegistry: all() ist nach Typ-ID sortiert, Zeiger bleiben gültig") {
    NodeRegistry registry;
    REQUIRE(registry.registerNode(minimal("m")).ok());
    REQUIRE(registry.registerNode(minimal("a")).ok());
    REQUIRE(registry.registerNode(minimal("z")).ok());

    const NodeDescriptor* before = registry.find("m");
    for (int i = 0; i < 50; ++i) {
        REQUIRE(registry.registerNode(minimal("extra." + std::to_string(i))).ok());
    }
    CHECK(registry.find("m") == before);

    const std::vector<const NodeDescriptor*> all = registry.all();
    REQUIRE(all.size() == 53U);
    CHECK(std::is_sorted(all.begin(), all.end(), [](const NodeDescriptor* a, const NodeDescriptor* b) {
        return a->typeId < b->typeId;
    }));
}
