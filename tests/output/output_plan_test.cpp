// OutputPlan: Verteilung der LEDs auf Universen und Füllen der Puffer.
#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

#include "whitehole/output/output_plan.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using namespace whitehole::output;

namespace {

using Bytes = std::vector<std::uint8_t>;

/// LedMap mit Fixtures 1, 2, 3 ... und den angegebenen LED-Zahlen, lückenlos hintereinander.
LedMap mapOf(const std::vector<std::uint32_t>& counts) {
    LedMap map;
    std::uint32_t first = 0;
    std::uint64_t id = 1;
    for (const std::uint32_t count : counts) {
        map.ranges.push_back({FixtureId{id++}, first, count});
        first += count;
    }
    map.world.assign(first, Vec3{});
    map.unit.assign(first, Vec3{});
    return map;
}

FixturePatch artnet(std::uint64_t fixture, std::uint16_t universe, std::uint16_t channel = 1) {
    FixturePatch p;
    p.fixture = FixtureId{fixture};
    p.protocol = Protocol::ArtNet;
    p.universe = universe;
    p.channel = channel;
    return p;
}

OutputPlan planOf(const LedMap& map, const std::vector<FixturePatch>& patches) {
    Result<OutputPlan> r = OutputPlan::create(map, patches);
    REQUIRE(r.ok());
    return std::move(r.value());
}

ErrorCode failureOf(const LedMap& map, const std::vector<FixturePatch>& patches) {
    const Result<OutputPlan> r = OutputPlan::create(map, patches);
    REQUIRE_FALSE(r.ok());
    return r.error().code;
}

}  // namespace

TEST_CASE("Ein kleines Fixture ergibt einen Slot", "[output][plan]") {
    const OutputPlan plan = planOf(mapOf({10}), {artnet(1, 3)});
    REQUIRE(plan.slots().size() == 1);
    const OutputSlot& slot = plan.slots()[0];
    CHECK(slot.protocol == Protocol::ArtNet);
    CHECK(slot.address == 3);
    CHECK(slot.length == 30);
    REQUIRE(slot.runs.size() == 1);
    CHECK(slot.runs[0].firstLed == 0);
    CHECK(slot.runs[0].ledCount == 10);
    CHECK(slot.runs[0].byteOffset == 0);
    CHECK(plan.ledCount() == 10);
}

TEST_CASE("Ein langes Fixture bricht auf das nächste Universe um", "[output][plan]") {
    const OutputPlan plan = planOf(mapOf({200}), {artnet(1, 5)});
    REQUIRE(plan.slots().size() == 2);
    CHECK(plan.slots()[0].address == 5);
    CHECK(plan.slots()[0].length == 510);  // 170 LEDs: keine LED wird geteilt
    CHECK(plan.slots()[0].runs[0].ledCount == 170);
    CHECK(plan.slots()[1].address == 6);
    CHECK(plan.slots()[1].length == 90);
    CHECK(plan.slots()[1].runs[0].firstLed == 170);
    CHECK(plan.slots()[1].runs[0].ledCount == 30);
    CHECK(plan.slots()[1].runs[0].byteOffset == 0);
}

TEST_CASE("Ein Startkanal verkürzt nur das erste Universe", "[output][plan]") {
    // Startkanal 101: 412 freie Kanäle -> 137 LEDs (411 Bytes), der Rest ab Kanal 1.
    const OutputPlan plan = planOf(mapOf({200}), {artnet(1, 0, 101)});
    REQUIRE(plan.slots().size() == 2);
    CHECK(plan.slots()[0].runs[0].byteOffset == 100);
    CHECK(plan.slots()[0].runs[0].ledCount == 137);
    CHECK(plan.slots()[0].length == 511);
    CHECK(plan.slots()[1].runs[0].ledCount == 63);
    CHECK(plan.slots()[1].runs[0].byteOffset == 0);
    CHECK(plan.slots()[1].length == 189);
}

TEST_CASE("Genau 170 LEDs passen in ein Universe, die 171. nicht", "[output][plan]") {
    CHECK(planOf(mapOf({170}), {artnet(1, 0)}).slots().size() == 1);
    CHECK(planOf(mapOf({171}), {artnet(1, 0)}).slots().size() == 2);
    // Der letzte erlaubte Startkanal 510 nimmt genau eine LED auf.
    const OutputPlan plan = planOf(mapOf({2}), {artnet(1, 0, 510)});
    REQUIRE(plan.slots().size() == 2);
    CHECK(plan.slots()[0].length == 512);
    CHECK(plan.slots()[1].length == 3);
}

TEST_CASE("Fixtures teilen sich ein Universe, wenn die Kanäle frei sind", "[output][plan]") {
    const OutputPlan plan = planOf(mapOf({10, 10}), {artnet(1, 0, 1), artnet(2, 0, 31)});
    REQUIRE(plan.slots().size() == 1);
    CHECK(plan.slots()[0].length == 60);
    REQUIRE(plan.slots()[0].runs.size() == 2);
    CHECK(plan.slots()[0].runs[1].firstLed == 10);
    CHECK(plan.slots()[0].runs[1].byteOffset == 30);
}

TEST_CASE("Die Slotlänge reicht bis zum letzten belegten Kanal, egal in welcher Reihenfolge gepatcht wird",
          "[output][plan]") {
    const OutputPlan plan = planOf(mapOf({10, 10}), {artnet(2, 0, 31), artnet(1, 0, 1)});
    REQUIRE(plan.slots().size() == 1);
    CHECK(plan.slots()[0].length == 60);
    CHECK(plan.slots()[0].runs.size() == 2);
}

TEST_CASE("Überlappende Kanäle werden abgelehnt", "[output][plan]") {
    CHECK(failureOf(mapOf({10, 10}), {artnet(1, 0, 1), artnet(2, 0, 30)}) == ErrorCode::AlreadyExists);
    // Auch in umgekehrter Reihenfolge und bei Überlauf aus dem Vorgänger-Universe.
    CHECK(failureOf(mapOf({10, 10}), {artnet(2, 0, 30), artnet(1, 0, 1)}) == ErrorCode::AlreadyExists);
    CHECK(failureOf(mapOf({200, 10}), {artnet(1, 0, 1), artnet(2, 1, 1)}) == ErrorCode::AlreadyExists);
}

TEST_CASE("Gleiches Universe an verschiedenen Zielen oder Protokollen sind getrennte Slots", "[output][plan]") {
    FixturePatch a = artnet(1, 0);
    FixturePatch b = artnet(2, 0);
    b.destination = 1;
    FixturePatch c = artnet(3, 1);
    c.protocol = Protocol::Sacn;
    c.universe = 1;
    const OutputPlan plan = planOf(mapOf({5, 5, 5}), {a, b, c});
    CHECK(plan.slots().size() == 3);
}

TEST_CASE("DDP erzeugt einen Strom ab dem Offset", "[output][plan]") {
    FixturePatch p;
    p.fixture = FixtureId{2};
    p.protocol = Protocol::Ddp;
    p.destination = 4;
    p.ddpOffset = 30;
    const OutputPlan plan = planOf(mapOf({7, 600}), {p});  // Fixture 1 bleibt ungepatcht
    REQUIRE(plan.slots().size() == 1);
    const OutputSlot& slot = plan.slots()[0];
    CHECK(slot.protocol == Protocol::Ddp);
    CHECK(slot.destination == 4);
    CHECK(slot.address == 30);
    CHECK(slot.length == 1800);
    REQUIRE(slot.runs.size() == 1);
    CHECK(slot.runs[0].firstLed == 7);
    CHECK(slot.runs[0].ledCount == 600);
}

TEST_CASE("Fixtures ohne LEDs erzeugen keinen Slot", "[output][plan]") {
    FixturePatch d;
    d.fixture = FixtureId{3};
    d.protocol = Protocol::Ddp;
    const OutputPlan plan = planOf(mapOf({4, 0, 0}), {artnet(1, 0), artnet(2, 1), d});
    CHECK(plan.slots().size() == 1);
}

TEST_CASE("Fehlerhafte Patches werden beim Erstellen erkannt", "[output][plan]") {
    const LedMap map = mapOf({10, 10});
    CHECK(failureOf(map, {artnet(9, 0)}) == ErrorCode::NotFound);
    CHECK(failureOf(map, {artnet(1, 0), artnet(1, 1)}) == ErrorCode::AlreadyExists);
    CHECK(failureOf(map, {artnet(1, 0, 0)}) == ErrorCode::InvalidArgument);
    CHECK(failureOf(map, {artnet(1, 0, 511)}) == ErrorCode::InvalidArgument);
    CHECK(failureOf(map, {artnet(1, 0x8000)}) == ErrorCode::InvalidArgument);

    FixturePatch sacn = artnet(1, 0);
    sacn.protocol = Protocol::Sacn;
    CHECK(failureOf(map, {sacn}) == ErrorCode::InvalidArgument);  // Universe 0 gibt es nicht
    sacn.universe = 64000;
    CHECK(failureOf(map, {sacn}) == ErrorCode::InvalidArgument);

    FixturePatch badColor = artnet(1, 0);
    badColor.color.gamma = 0.0F;
    CHECK(failureOf(map, {badColor}) == ErrorCode::InvalidArgument);
}

TEST_CASE("Der Umbruch endet am letzten gültigen Universe", "[output][plan]") {
    CHECK(planOf(mapOf({170}), {artnet(1, 32767)}).slots().size() == 1);
    CHECK(failureOf(mapOf({171}), {artnet(1, 32767)}) == ErrorCode::InvalidArgument);

    FixturePatch sacn = artnet(1, 63999);
    sacn.protocol = Protocol::Sacn;
    CHECK(planOf(mapOf({170}), {sacn}).slots().size() == 1);
    CHECK(failureOf(mapOf({171}), {sacn}) == ErrorCode::InvalidArgument);
}

TEST_CASE("Ungültige Universes fallen auch bei Fixtures ohne LEDs auf", "[output][plan]") {
    CHECK(failureOf(mapOf({0}), {artnet(1, 0x8000)}) == ErrorCode::InvalidArgument);
    FixturePatch sacn = artnet(1, 0);
    sacn.protocol = Protocol::Sacn;
    CHECK(failureOf(mapOf({0}), {sacn}) == ErrorCode::InvalidArgument);
}

TEST_CASE("Der DDP-Strom darf 32 Bit nicht überschreiten", "[output][plan]") {
    FixturePatch p;
    p.fixture = FixtureId{1};
    p.protocol = Protocol::Ddp;
    p.ddpOffset = 0xFFFFFFFDU;
    CHECK(failureOf(mapOf({2}), {p}) == ErrorCode::InvalidArgument);
    p.ddpOffset = 0xFFFFFFFCU;
    CHECK(planOf(mapOf({1}), {p}).slots().size() == 1);
}

TEST_CASE("render schreibt Bytes in Kabelreihenfolge und Farbreihenfolge", "[output][plan]") {
    FixturePatch grb = artnet(1, 0);
    grb.color.order = ColorOrder::GRB;
    grb.color.gamma = 1.0F;
    const OutputPlan plan = planOf(mapOf({3}), {grb});

    const std::vector<ColorRgba> colors{{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
    std::vector<Bytes> out;
    REQUIRE(plan.render(colors, out).ok());
    REQUIRE(out.size() == 1);
    // GRB: Rot -> (g=0, r=255, b=0), Grün -> (255, 0, 0), Blau -> (0, 0, 255)
    CHECK(out[0] == Bytes{0, 255, 0, 255, 0, 0, 0, 0, 255});
}

TEST_CASE("render lässt Lücken leer und nutzt je Fixture die eigenen Einstellungen", "[output][plan]") {
    FixturePatch a = artnet(1, 0, 1);
    a.color.gamma = 1.0F;
    FixturePatch b = artnet(2, 0, 7);  // zwei Kanäle Lücke nach Fixture 1 (Kanäle 1..3) -> 4..6 leer
    b.color.gamma = 1.0F;
    b.color.order = ColorOrder::BGR;
    b.color.brightness = 0.0F;
    FixturePatch c = artnet(3, 0, 10);
    c.color.gamma = 1.0F;
    c.color.order = ColorOrder::BGR;

    const OutputPlan plan = planOf(mapOf({1, 1, 1}), {a, b, c});
    const std::vector<ColorRgba> colors{{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 0, 0, 1}};
    std::vector<Bytes> out;
    REQUIRE(plan.render(colors, out).ok());
    REQUIRE(out.size() == 1);
    // Fixture 1: weiß; Kanäle 4..6 leer; Fixture 2: Helligkeit 0 -> schwarz; Fixture 3: rot als BGR
    CHECK(out[0] == Bytes{255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 255});
}

TEST_CASE("render prüft die Farbzahl und behält die Puffer", "[output][plan]") {
    const OutputPlan plan = planOf(mapOf({4}), {artnet(1, 0)});
    std::vector<Bytes> out;
    const std::vector<ColorRgba> tooFew(3);
    const Result<void> bad = plan.render(tooFew, out);
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == ErrorCode::InvalidArgument);

    const std::vector<ColorRgba> colors(4, ColorRgba{1, 1, 1, 1});
    REQUIRE(plan.render(colors, out).ok());
    const std::uint8_t* before = out[0].data();
    REQUIRE(plan.render(colors, out).ok());
    CHECK(out[0].data() == before);  // keine neue Allokation im zweiten Frame
    CHECK(out[0].size() == 12);
}

TEST_CASE("Ein Plan ohne Patches ist leer und gültig", "[output][plan]") {
    const OutputPlan plan = planOf(mapOf({5}), {});
    CHECK(plan.slots().empty());
    std::vector<Bytes> out(3);
    REQUIRE(plan.render(std::vector<ColorRgba>(5), out).ok());
    CHECK(out.empty());
}
