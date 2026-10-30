// OutputPipeline: Pakete, Sequenznummern, Ziele und Fehlerverhalten (ohne Netzwerk).
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <utility>
#include <vector>

#include "recording_sink.hpp"
#include "whitehole/output/output_pipeline.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using namespace whitehole::output;
using whitehole::testing::RecordingSink;

namespace {

using Bytes = std::vector<std::uint8_t>;

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

FixturePatch patch(std::uint64_t fixture, Protocol protocol, std::uint16_t universe, std::uint32_t destination = 0) {
    FixturePatch p;
    p.fixture = FixtureId{fixture};
    p.protocol = protocol;
    p.universe = universe;
    p.destination = destination;
    p.color.gamma = 1.0F;
    return p;
}

struct Rig {
    RecordingSink sink;
    std::unique_ptr<OutputPipeline> pipeline;
};

std::unique_ptr<Rig> rig(const LedMap& map, const std::vector<FixturePatch>& patches, OutputConfig config) {
    auto r = std::make_unique<Rig>();
    Result<OutputPlan> plan = OutputPlan::create(map, patches);
    REQUIRE(plan.ok());
    Result<std::unique_ptr<OutputPipeline>> pipeline =
        OutputPipeline::create(std::move(plan.value()), std::move(config), r->sink);
    REQUIRE(pipeline.ok());
    r->pipeline = std::move(pipeline.value());
    return r;
}

OutputConfig oneDestination(const std::string& address, std::uint16_t port = 0) {
    OutputConfig c;
    c.destinations.push_back({address, port});
    return c;
}

const std::vector<ColorRgba> kRedBlue{{1, 0, 0, 1}, {0, 0, 1, 1}};

}  // namespace

TEST_CASE("Art-Net: genau ein Paket mit den erwarteten Bytes und Zielen", "[output][pipeline]") {
    auto r = rig(mapOf({2}), {patch(1, Protocol::ArtNet, 0x0102)}, oneDestination("192.168.1.50"));
    REQUIRE(r->pipeline->process(kRedBlue).ok());

    const auto sent = r->sink.packets();
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].to == Endpoint{"192.168.1.50", 6454});
    const Bytes expected{'A', 'r', 't', '-', 'N', 'e', 't', 0, 0x00, 0x50, 0x00, 0x0E, 1, 0,
                         0x02, 0x01, 0x00, 0x06, 255, 0, 0, 0, 0, 255};
    CHECK(sent[0].bytes == expected);
    CHECK(r->pipeline->stats().framesProcessed == 1);
    CHECK(r->pipeline->stats().packetsSent == 1);
    CHECK(r->pipeline->stats().sendErrors == 0);
}

TEST_CASE("Art-Net: Sequenz zählt hoch und überspringt die 0", "[output][pipeline]") {
    auto r = rig(mapOf({2}), {patch(1, Protocol::ArtNet, 0)}, oneDestination("10.0.0.1"));
    for (int i = 0; i < 257; ++i) {
        REQUIRE(r->pipeline->process(kRedBlue).ok());
    }
    const auto sent = r->sink.packets();
    REQUIRE(sent.size() == 257);
    CHECK(sent[0].bytes[12] == 1);
    CHECK(sent[1].bytes[12] == 2);
    CHECK(sent[254].bytes[12] == 255);
    CHECK(sent[255].bytes[12] == 1);  // 0 würde "Sequenz aus" heißen
    CHECK(sent[256].bytes[12] == 2);
}

TEST_CASE("sACN: Multicast bei leerer Adresse, Sequenz ab 0 mit Überlauf", "[output][pipeline]") {
    OutputConfig config = oneDestination("");
    config.cid = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    config.sacnPriority = 120;
    config.sourceName = "Probe";
    auto r = rig(mapOf({2}), {patch(1, Protocol::Sacn, 5)}, std::move(config));
    for (int i = 0; i < 258; ++i) {
        REQUIRE(r->pipeline->process(kRedBlue).ok());
    }
    const auto sent = r->sink.packets();
    REQUIRE(sent.size() == 258);
    CHECK(sent[0].to == Endpoint{"239.255.0.5", 5568});
    CHECK(sent[0].bytes[22] == 1);    // CID
    CHECK(sent[0].bytes[37] == 16);
    CHECK(sent[0].bytes[44] == 'P');  // Quellname
    CHECK(sent[0].bytes[108] == 120);
    CHECK(sent[0].bytes[113] == 0);   // Universe 5
    CHECK(sent[0].bytes[114] == 5);
    CHECK(sent[0].bytes[126] == 255);
    CHECK(sent[0].bytes[111] == 0);
    CHECK(sent[1].bytes[111] == 1);
    CHECK(sent[255].bytes[111] == 255);
    CHECK(sent[256].bytes[111] == 0);
    CHECK(sent[257].bytes[111] == 1);
}

TEST_CASE("sACN mit fester Adresse und eigenem Port", "[output][pipeline]") {
    auto r = rig(mapOf({2}), {patch(1, Protocol::Sacn, 1)}, oneDestination("10.1.2.3", 6000));
    REQUIRE(r->pipeline->process(kRedBlue).ok());
    CHECK(r->sink.packets()[0].to == Endpoint{"10.1.2.3", 6000});
}

TEST_CASE("DDP: Strom in Paketen, Push nur am Ende, Sequenz 1 bis 15", "[output][pipeline]") {
    FixturePatch p = patch(1, Protocol::Ddp, 0);
    p.ddpOffset = 3;
    auto r = rig(mapOf({600}), {p}, oneDestination("10.0.0.9", 4049));
    const std::vector<ColorRgba> white(600, ColorRgba{1, 1, 1, 1});
    for (int i = 0; i < 16; ++i) {
        REQUIRE(r->pipeline->process(white).ok());
    }
    const auto sent = r->sink.packets();
    REQUIRE(sent.size() == 32);  // 1800 Bytes = 1440 + 360 je Frame
    CHECK(sent[0].to == Endpoint{"10.0.0.9", 4049});
    CHECK(sent[0].bytes.size() == 10 + 1440);
    CHECK(sent[1].bytes.size() == 10 + 360);
    CHECK(sent[0].bytes[0] == 0x40);
    CHECK(sent[1].bytes[0] == 0x41);
    CHECK(sent[0].bytes[7] == 3);           // Offset 3
    CHECK(sent[1].bytes[7] == (3 + 1440) % 256);
    CHECK(sent[0].bytes[1] == 1);
    CHECK(sent[1].bytes[1] == 1);           // beide Pakete eines Frames teilen die Sequenz
    CHECK(sent[2].bytes[1] == 2);
    CHECK(sent[28].bytes[1] == 15);
    CHECK(sent[30].bytes[1] == 1);          // nach 15 wieder 1
    CHECK(sent[0].bytes[10] == 255);
}

TEST_CASE("Ohne Port gelten die Standardports der Protokolle", "[output][pipeline]") {
    OutputConfig config;
    config.destinations = {{"10.0.0.1", 0}};
    auto r = rig(mapOf({1, 1, 1}),
                 {patch(1, Protocol::ArtNet, 0), patch(2, Protocol::Sacn, 1), patch(3, Protocol::Ddp, 0)},
                 std::move(config));
    REQUIRE(r->pipeline->process(std::vector<ColorRgba>(3)).ok());
    const auto sent = r->sink.packets();
    REQUIRE(sent.size() == 3);
    CHECK(sent[0].to.port == 6454);
    CHECK(sent[1].to.port == 5568);
    CHECK(sent[2].to.port == 4048);
}

TEST_CASE("Mehrere Universen und Ziele werden getrennt bedient", "[output][pipeline]") {
    OutputConfig config;
    config.destinations = {{"10.0.0.1", 0}, {"10.0.0.2", 0}};
    auto r = rig(mapOf({200, 4}), {patch(1, Protocol::ArtNet, 0, 0), patch(2, Protocol::ArtNet, 0, 1)},
                 std::move(config));
    const std::vector<ColorRgba> colors(204, ColorRgba{0, 1, 0, 1});
    REQUIRE(r->pipeline->process(colors).ok());
    const auto sent = r->sink.packets();
    REQUIRE(sent.size() == 3);
    CHECK(sent[0].to.address == "10.0.0.1");
    CHECK(sent[0].bytes.size() == 18 + 510);
    CHECK(sent[1].to.address == "10.0.0.1");
    CHECK(sent[1].bytes[14] == 1);  // Universe 1
    CHECK(sent[2].to.address == "10.0.0.2");
    CHECK(sent[2].bytes[14] == 0);
}

TEST_CASE("Ein ausgefallenes Ziel stoppt die anderen nicht", "[output][pipeline]") {
    OutputConfig config;
    config.destinations = {{"10.0.0.1", 0}, {"10.0.0.2", 0}};
    auto r = rig(mapOf({2, 2}), {patch(1, Protocol::ArtNet, 0, 0), patch(2, Protocol::ArtNet, 1, 1)},
                 std::move(config));
    r->sink.failFor("10.0.0.1");
    const std::vector<ColorRgba> colors(4, ColorRgba{1, 1, 1, 1});
    for (int i = 0; i < 3; ++i) {
        REQUIRE(r->pipeline->process(colors).ok());  // der Frame selbst gelingt
    }
    const OutputStats stats = r->pipeline->stats();
    CHECK(stats.framesProcessed == 3);
    CHECK(stats.sendErrors == 3);
    CHECK(stats.packetsSent == 3);
    REQUIRE(r->pipeline->lastSendError().has_value());
    CHECK(r->pipeline->lastSendError()->code == ErrorCode::IoError);
    for (const auto& p : r->sink.packets()) {
        CHECK(p.to.address == "10.0.0.2");
    }
}

TEST_CASE("processBytes liest 16 Byte je LED und erlaubt Reserve", "[output][pipeline]") {
    auto r = rig(mapOf({2}), {patch(1, Protocol::ArtNet, 0)}, oneDestination("10.0.0.1"));
    std::vector<std::byte> bytes(2 * sizeof(ColorRgba) + 32, std::byte{0xEE});  // Reserve dahinter
    std::memcpy(bytes.data(), kRedBlue.data(), 2 * sizeof(ColorRgba));
    REQUIRE(r->pipeline->processBytes(bytes).ok());
    const auto sent = r->sink.packets();
    REQUIRE(sent.size() == 1);
    CHECK(Bytes(sent[0].bytes.begin() + 18, sent[0].bytes.end()) == Bytes{255, 0, 0, 0, 0, 255});
}

TEST_CASE("Ein zu kleiner Frame sendet nichts", "[output][pipeline]") {
    auto r = rig(mapOf({2}), {patch(1, Protocol::ArtNet, 0)}, oneDestination("10.0.0.1"));
    const std::vector<std::byte> bytes(31);
    const Result<void> bad = r->pipeline->processBytes(bytes);
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == ErrorCode::InvalidArgument);
    CHECK(r->sink.count() == 0);
    CHECK(r->pipeline->stats().framesProcessed == 0);
    CHECK_FALSE(r->pipeline->process(std::vector<ColorRgba>(3)).ok());
}

TEST_CASE("Ein Plan ohne Slots läuft leer durch", "[output][pipeline]") {
    auto r = rig(mapOf({3}), {}, OutputConfig{});
    REQUIRE(r->pipeline->process(std::vector<ColorRgba>(3)).ok());
    CHECK(r->sink.count() == 0);
    CHECK(r->pipeline->stats().framesProcessed == 1);
}

TEST_CASE("Falsche Ziele werden beim Erstellen abgelehnt", "[output][pipeline]") {
    RecordingSink sink;
    const LedMap map = mapOf({2});
    auto create = [&](const FixturePatch& p, OutputConfig c) {
        Result<OutputPlan> plan = OutputPlan::create(map, std::vector<FixturePatch>{p});
        REQUIRE(plan.ok());
        return OutputPipeline::create(std::move(plan.value()), std::move(c), sink);
    };
    const FixturePatch art = patch(1, Protocol::ArtNet, 0);

    CHECK(create(art, OutputConfig{}).error().code == ErrorCode::InvalidArgument);  // Ziel 0 fehlt
    CHECK(create(art, oneDestination("")).error().code == ErrorCode::InvalidArgument);
    CHECK(create(art, oneDestination("controller.local")).error().code == ErrorCode::InvalidArgument);
    CHECK(create(art, oneDestination("256.0.0.1")).error().code == ErrorCode::InvalidArgument);
    CHECK(create(art, oneDestination("192.168.1.1")).ok());
    CHECK(create(patch(1, Protocol::Ddp, 0), oneDestination("")).error().code == ErrorCode::InvalidArgument);
    CHECK(create(patch(1, Protocol::Sacn, 1), oneDestination("")).ok());

    OutputConfig loud = oneDestination("10.0.0.1");
    loud.sacnPriority = 201;
    CHECK(create(art, loud).error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("isIpv4 erkennt nur vierteilige Zahlenadressen", "[output][pipeline]") {
    for (const char* ok : {"0.0.0.0", "192.168.1.50", "255.255.255.255", "10.0.0.1"}) {
        CHECK(isIpv4(ok));
    }
    for (const char* bad : {"", "1.2.3", "1.2.3.4.5", "1.2.3.", ".1.2.3", "1..2.3", "256.1.1.1", "1.2.3.1000",
                            "a.b.c.d", "1.2.3.4 ", "localhost", "1.2.3.-4", "::1"}) {
        CHECK_FALSE(isIpv4(bad));
    }
}
