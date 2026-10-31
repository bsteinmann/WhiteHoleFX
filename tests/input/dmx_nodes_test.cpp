// Die DMX-Nodes: "DMX Channel" und "DMX Color" lesen aus dem gemeinsamen DmxStore.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <variant>
#include <vector>

#include "builtin_lab.hpp"

using namespace whitehole;
using Catch::Approx;
using input::DmxProtocol;
using whitehole::test::Lab;

namespace {

bool flag(Lab& lab, NodeId node, const char* port) {
    const auto value = lab.store.value(node, port);
    REQUIRE(value.has_value());
    return std::get<bool>(*value);
}

NodeId channelNode(Lab& lab) {
    const NodeId node = lab.add("wh.value.dmx");
    lab.feed(node, "value");
    const NodeId sink = lab.add("wh.transform.rotate");  // zweiter Verbraucher für die Schalter
    lab.connect(node, "present", sink, "angle");
    return node;
}

}  // namespace

TEST_CASE("DMX Channel: 8 Bit wird auf Min..Max abgebildet", "[input][nodes]") {
    Lab lab;
    const NodeId node = channelNode(lab);
    lab.set(node, "universe", std::int64_t{3});
    lab.set(node, "channel", std::int64_t{2});
    lab.set(node, "min", -1.0);
    lab.set(node, "max", 1.0);

    lab.dmx->write(DmxProtocol::ArtNet, 3, std::vector<std::uint8_t>{0, 255, 0});
    CHECK(lab.sample(node, 0.0, "value") == Approx(1.0));
    CHECK(flag(lab, node, "present"));
    CHECK(flag(lab, node, "on"));

    lab.dmx->write(DmxProtocol::ArtNet, 3, std::vector<std::uint8_t>{0, 51, 0});
    CHECK(lab.sample(node, 0.1, "value") == Approx(-1.0 + 2.0 * 0.2));
    CHECK_FALSE(flag(lab, node, "on"));  // 0.2 < Schwelle 0.5
}

TEST_CASE("DMX Channel: 16 Bit nimmt Kanal und Folgekanal", "[input][nodes]") {
    Lab lab;
    const NodeId node = channelNode(lab);
    lab.set(node, "bits16", true);
    lab.set(node, "channel", std::int64_t{1});

    lab.dmx->write(DmxProtocol::ArtNet, 1, std::vector<std::uint8_t>{0x80, 0x00});
    CHECK(lab.sample(node, 0.0, "value") == Approx(32768.0 / 65535.0));
    lab.dmx->write(DmxProtocol::ArtNet, 1, std::vector<std::uint8_t>{0xFF, 0xFF});
    CHECK(lab.sample(node, 0.1, "value") == Approx(1.0));
}

TEST_CASE("DMX Channel: Quelle sACN liest nur sACN-Pakete", "[input][nodes]") {
    Lab lab;
    const NodeId node = channelNode(lab);
    lab.set(node, "source", std::int64_t{1});

    lab.dmx->write(DmxProtocol::ArtNet, 1, std::vector<std::uint8_t>{255});
    CHECK(lab.sample(node, 0.0, "value") == Approx(0.0));
    CHECK_FALSE(flag(lab, node, "present"));

    lab.dmx->write(DmxProtocol::Sacn, 1, std::vector<std::uint8_t>{255});
    CHECK(lab.sample(node, 0.1, "value") == Approx(1.0));
    CHECK(flag(lab, node, "present"));
}

TEST_CASE("DMX Channel: Signalverlust hält den Wert oder fällt auf 0", "[input][nodes]") {
    auto now = std::chrono::steady_clock::time_point{} + std::chrono::seconds{10};
    Lab lab;
    lab.dmx = std::make_shared<input::DmxStore>([&now] { return now; });
    // Ein Lab mit eigener Uhr: Kernel neu an den frischen Speicher binden.
    lab.kernels = values::CpuKernelLibrary{};
    for (const auto& definition : nodes::builtinCpuDefinitions(lab.dmx)) {
        REQUIRE(lab.kernels.add(definition.descriptor.typeId, definition.factory).ok());
    }
    const NodeId node = channelNode(lab);
    lab.set(node, "timeout", 1.0);

    lab.dmx->write(DmxProtocol::ArtNet, 1, std::vector<std::uint8_t>{204});
    CHECK(lab.sample(node, 0.0, "value") == Approx(0.8));
    CHECK(flag(lab, node, "present"));

    now += std::chrono::seconds{2};  // älter als das Zeitlimit
    CHECK(lab.sample(node, 0.1, "value") == Approx(0.8));  // gehalten
    CHECK_FALSE(flag(lab, node, "present"));

    lab.set(node, "hold", false);
    CHECK(lab.sample(node, 0.2, "value") == Approx(0.0));

    lab.dmx->write(DmxProtocol::ArtNet, 1, std::vector<std::uint8_t>{255});  // Signal kehrt zurück
    CHECK(lab.sample(node, 0.3, "value") == Approx(1.0));
    CHECK(flag(lab, node, "present"));

    now += std::chrono::seconds{100};  // Zeitlimit 0 = nie ablaufen
    lab.set(node, "timeout", 0.0);
    lab.run(0.4);
    CHECK(flag(lab, node, "present"));
}

TEST_CASE("DMX Color: RGB und RGB mit Dimmer", "[input][nodes]") {
    Lab lab;
    const NodeId node = lab.add("wh.value.dmxcolor");
    const NodeId sink = lab.add("wh.generator.solid");
    lab.connect(node, "out", sink, "color");
    lab.set(node, "channel", std::int64_t{10});

    std::vector<std::uint8_t> data(20, 0);
    data[9] = 255;
    data[10] = 128;
    data[11] = 0;
    data[12] = 128;  // Dimmer
    lab.dmx->write(DmxProtocol::ArtNet, 1, data);

    lab.run(0.0);
    ColorRgba color = std::get<ColorRgba>(*lab.store.value(node, "out"));
    CHECK(color.r == Approx(1.0F));
    CHECK(color.g == Approx(128.0F / 255.0F));
    CHECK(color.b == Approx(0.0F));
    CHECK(color.a == Approx(1.0F));

    lab.set(node, "mode", std::int64_t{1});
    lab.run(0.1);
    color = std::get<ColorRgba>(*lab.store.value(node, "out"));
    CHECK(color.r == Approx(128.0F / 255.0F));
    CHECK(color.g == Approx(128.0F / 255.0F * 128.0F / 255.0F));
}

TEST_CASE("DMX-Nodes ohne Speicher: dauerhaft kein Signal, kein Absturz", "[input][nodes]") {
    graph::NodeRegistry registry;
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    REQUIRE(nodes::registerBuiltinNodes(registry, snippets, kernels).ok());
    CHECK(kernels.create("wh.value.dmx") != nullptr);
    CHECK(kernels.create("wh.value.dmxcolor") != nullptr);
}
