// DMX Out und DMX Color Out: Senken ohne Ausgang, die je Frame einen ChannelWrite in den Store legen.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "builtin_lab.hpp"

using namespace whitehole;
using whitehole::test::Lab;

namespace {

const ChannelWrite& writeOf(const Lab& lab, NodeId node) {
    const auto& table = lab.store.allChannels();
    const auto it = table.find(node);
    REQUIRE(it != table.end());
    return it->second;
}

}  // namespace

TEST_CASE("DMX Out: läuft ohne Verbraucher, skaliert 0..1 auf low..high, 16 Bit belegt zwei Kanäle", "[nodes][dmx]") {
    Lab lab;
    const NodeId node = lab.add("wh.output.dmx");
    lab.set(node, "destination", std::int64_t{2});
    lab.set(node, "universe", std::int64_t{7});
    lab.set(node, "channel", std::int64_t{100});
    lab.set(node, "count", std::int64_t{3});
    lab.set(node, "ch1", 1.0);
    lab.set(node, "ch2", 0.5);
    lab.run(0.0);
    const ChannelWrite& w = writeOf(lab, node);
    CHECK(w.destination == 2);
    CHECK(w.universe == 7);
    CHECK(w.firstChannel == 100);
    CHECK(w.data == std::vector<std::uint8_t>{255, 128, 0});

    lab.set(node, "low", 10.0);
    lab.set(node, "high", 210.0);
    lab.run(0.1);
    CHECK(writeOf(lab, node).data == std::vector<std::uint8_t>{210, 110, 10});

    lab.set(node, "bits16", true);
    lab.run(0.2);
    // 210 * 257 = 53970 = 0xD2D2, 110 * 257 = 28270 = 0x6E6E, 10 * 257 = 0x0A0A
    CHECK(writeOf(lab, node).data == std::vector<std::uint8_t>{0xD2, 0xD2, 0x6E, 0x6E, 0x0A, 0x0A});
}

TEST_CASE("DMX Out: ein modulierter Eingang kommt an", "[nodes][dmx]") {
    Lab lab;
    const NodeId lfo = lab.add("wh.value.lfo");
    const NodeId node = lab.add("wh.output.dmx");
    lab.set(node, "count", std::int64_t{1});
    lab.connect(lfo, "out", node, "ch1");
    lab.run(0.0);
    const std::uint8_t first = writeOf(lab, node).data.at(0);
    lab.run(0.3);
    CHECK(writeOf(lab, node).data.at(0) != first);  // der LFO bewegt den Kanal
}

TEST_CASE("DMX Color Out: RGB, RGBW mit Weiß als gemeinsamem Anteil, Dimmer davor", "[nodes][dmx]") {
    Lab lab;
    const NodeId node = lab.add("wh.output.dmxcolor");
    lab.set(node, "color", ColorRgba{1.0F, 0.5F, 0.25F, 1.0F});
    lab.run(0.0);
    CHECK(writeOf(lab, node).data == std::vector<std::uint8_t>{255, 128, 64});
    lab.set(node, "mode", std::int64_t{1});
    lab.run(0.1);
    CHECK(writeOf(lab, node).data == std::vector<std::uint8_t>{255, 128, 64, 64});
    lab.set(node, "mode", std::int64_t{3});
    lab.set(node, "dimmer", 0.5);
    lab.run(0.2);
    CHECK(writeOf(lab, node).data == std::vector<std::uint8_t>{128, 255, 128, 64, 64});
    lab.set(node, "mode", std::int64_t{2});
    lab.set(node, "color", ColorRgba{1.0F, 1.0F, 1.0F, 0.0F});  // Alpha 0: Farbe aus, Dimmer bleibt
    lab.run(0.3);
    CHECK(writeOf(lab, node).data == std::vector<std::uint8_t>{128, 0, 0, 0});
}
