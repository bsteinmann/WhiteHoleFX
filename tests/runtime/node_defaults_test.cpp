// Eigene Standardwerte für neue Nodes: Kodierung, Verwaltung und die Wirkung beim Anlegen im Player.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <variant>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/node_defaults.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using namespace whitehole::runtime;

TEST_CASE("NodeDefaults: Werte als Text hin und zurück", "[defaults]") {
    const graph::ParameterValue values[] = {
        true, false, std::int64_t{-5}, 0.25, 1.0e-7, Vec2{1.5F, -2.0F}, Vec3{0.1F, 0.2F, 0.3F},
        ColorRgba{1.0F, 0.5F, 0.0F, 1.0F}, std::string{"Hallo, Welt: 1"}, std::string{}};
    for (const auto& value : values) {
        const std::string text = encodeParameterValue(value);
        const auto back = decodeParameterValue(text);
        REQUIRE(back.has_value());
        CHECK(*back == value);
    }
    CHECK(encodeParameterValue(0.25) == "d:0.25");
    CHECK(encodeParameterValue(true) == "b:1");
}

TEST_CASE("NodeDefaults: Unsinn wird beim Lesen abgelehnt", "[defaults]") {
    for (const char* bad : {"", "x", "d:", "d:abc", "d:nan", "d:inf", "d:1x", "i:1.5", "b:2", "v2:1", "v2:1,2,3", "v3:1,,2",
                            "c:1,2,3", "q:1"}) {
        CHECK_FALSE(decodeParameterValue(bad).has_value());
    }
}

TEST_CASE("NodeDefaults: setzen, finden, entfernen", "[defaults]") {
    NodeDefaults defaults;
    CHECK(defaults.empty());
    defaults.set("a", "x", 1.0);
    defaults.set("a", "y", true);
    defaults.set("b", "x", 2.0);
    REQUIRE(defaults.find("a", "x") != nullptr);
    CHECK(*defaults.find("a", "x") == graph::ParameterValue{1.0});
    CHECK(defaults.find("a", "z") == nullptr);
    CHECK(defaults.find("c", "x") == nullptr);
    defaults.set("a", "x", 3.0);  // ersetzt
    CHECK(*defaults.find("a", "x") == graph::ParameterValue{3.0});

    CHECK(defaults.clear("a", "x"));
    CHECK_FALSE(defaults.clear("a", "x"));
    CHECK(defaults.clear("a", "y"));
    CHECK(defaults.valuesOf("a") == nullptr);  // leerer Typ verschwindet
    CHECK(defaults.clearType("b"));
    CHECK(defaults.empty());
}

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
  at 100 40
end
output plasma.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    test::FakeGpuDevice device;
    testing::RecordingSink recording;
    GatedPacketSink gate{recording};
    Player player{&device, *library, gate};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
    }
    [[nodiscard]] const graph::ParameterValue* parameter(NodeId id, const std::string& key) const {
        const graph::NodeInstance* node = player.project()->graph.findNode(id);
        REQUIRE(node != nullptr);
        const auto found = node->parameters.find(key);
        return found == node->parameters.end() ? nullptr : &found->second;
    }
};

}  // namespace

TEST_CASE("NodeDefaults: neue Nodes starten mit den eigenen Werten", "[defaults][player]") {
    Rig rig;
    NodeDefaults defaults;
    defaults.set("wh.generator.noise", "density", 0.3);
    defaults.set("wh.generator.noise", "gibt_es_nicht", 1.0);       // unbekannter Parameter: übersprungen
    defaults.set("wh.generator.noise", "scale", std::string{"x"});  // falscher Typ: übersprungen
    rig.player.setNodeDefaults(defaults);
    CHECK(rig.player.nodeDefaults() == defaults);

    const auto id = rig.player.addNode("wh.generator.noise", Vec2{0.0F, 0.0F});
    REQUIRE(id.ok());
    const auto* density = rig.parameter(id.value(), "density");
    REQUIRE(density != nullptr);
    CHECK(std::get<double>(*density) == 0.3);
    const auto* scale = rig.parameter(id.value(), "scale");
    REQUIRE(scale != nullptr);
    CHECK(std::holds_alternative<double>(*scale));

    // Andere Typen und der Zustand ohne Standardwerte bleiben bei der Bibliothek.
    const auto plasma = rig.player.addNode("wh.generator.plasma", Vec2{0.0F, 0.0F});
    REQUIRE(plasma.ok());
    rig.player.setNodeDefaults({});
    const auto plain = rig.player.addNode("wh.generator.noise", Vec2{0.0F, 0.0F});
    REQUIRE(plain.ok());
    CHECK(std::get<double>(*rig.parameter(plain.value(), "density")) != 0.3);
    // Der früher angelegte Node behält seinen Wert.
    CHECK(std::get<double>(*rig.parameter(id.value(), "density")) == 0.3);
}
