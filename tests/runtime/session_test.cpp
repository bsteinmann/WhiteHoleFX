// Tests der Session auf dem FakeGpuDevice. Das Fake rechnet keine Shader: Die Farben sind
// Nullen. Geprüft wird die Verdrahtung (Frames laufen vom Engine-Takt bis zum Paket) und das
// Fehlerverhalten beim Aufbau. Die Farbwerte prüft der GPU-Test.
#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <memory>
#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/session.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
destination 1 multicast

fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1 channel 1
end
fixture Strip at 10 0 0
  strip 100 from 0 0 0 to 10 0 0
  patch sacn destination 1 universe 3 channel 1
end

node g wh.generator.gradient
end
output g.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink sink;
    engine::ManualFrameClock clock{Seconds{1.0 / 40.0}};

    project::Project load(const std::string& text) {
        auto parsed = project::parseProject(text, library->registry);
        REQUIRE(parsed.ok());
        return std::move(parsed).value();
    }

    Result<std::unique_ptr<runtime::Session>> open(const project::Project& p) {
        return runtime::Session::create(
            p, runtime::SessionDeps{device, library->snippets, library->kernels, sink, clock});
    }
};

}  // namespace

TEST_CASE("buildOutput: ein Projekt ohne Fixtures oder ohne Patches wird abgelehnt", "[runtime]") {
    Rig rig;
    RecordingSink sink;
    project::Project empty{rig.library->registry};
    auto r = runtime::buildOutput(empty, sink);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("kein Fixture") != std::string::npos);

    project::Project unpatched = rig.load("whitehole-project 1\nfixture A\n  matrix 2 2\nend\n");
    r = runtime::buildOutput(unpatched, sink);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("gepatcht") != std::string::npos);
}

TEST_CASE("buildOutput: Konfigurationsfehler beim Patchen fallen vor dem Start auf", "[runtime]") {
    Rig rig;
    RecordingSink sink;
    // Ziel 5 gibt es nicht.
    project::Project p = rig.load(
        "whitehole-project 1\ndestination 0 10.0.0.5\nfixture A\n  matrix 2 2\n  patch artnet destination 5\nend\n");
    CHECK_FALSE(runtime::buildOutput(p, sink).ok());

    // Zwei Fixtures auf denselben Kanälen.
    p = rig.load(
        "whitehole-project 1\ndestination 0 10.0.0.5\n"
        "fixture A\n  matrix 2 2\n  patch artnet destination 0 universe 1\nend\n"
        "fixture B\n  matrix 2 2\n  patch artnet destination 0 universe 1\nend\n");
    CHECK_FALSE(runtime::buildOutput(p, sink).ok());
}

TEST_CASE("buildOutput: Karte und Pipeline passen zum Layout", "[runtime]") {
    Rig rig;
    RecordingSink sink;
    const project::Project p = rig.load(kProject);
    auto parts = runtime::buildOutput(p, sink);
    REQUIRE(parts.ok());
    CHECK(parts.value().map.world.size() == 108);
    CHECK(parts.value().pipeline->plan().ledCount() == 108);
}

TEST_CASE("Session: Frames laufen bis zu den Paketen", "[runtime]") {
    Rig rig;
    const project::Project p = rig.load(kProject);
    auto session = rig.open(p);
    REQUIRE(session.ok());
    CHECK(session.value()->ledCount() == 108);

    for (int i = 0; i < 5; ++i) {
        REQUIRE(session.value()->renderFrame().ok());
    }
    REQUIRE(session.value()->flush(std::chrono::seconds{5}));

    const runtime::SessionStats stats = session.value()->stats();
    CHECK(stats.engine.framesRendered + stats.engine.framesDropped == 5);
    CHECK(stats.output.framesProcessed + stats.mailboxDrops == stats.engine.framesRendered);
    CHECK(stats.frameErrors == 0);
    CHECK(stats.output.sendErrors == 0);
    CHECK_FALSE(session.value()->lastOutputError().has_value());
    CHECK_FALSE(session.value()->stageError().has_value());

    // Je Frame: ein Art-Net-Paket für das Panel (8 LEDs) und ein sACN-Paket für den Strip
    // (100 LEDs passen in ein Universe).
    const auto packets = rig.sink.packets();
    CHECK(packets.size() == 2 * stats.output.framesProcessed);
    std::size_t artnet = 0;
    std::size_t sacn = 0;
    for (const auto& packet : packets) {
        if (std::memcmp(packet.bytes.data(), "Art-Net", 7) == 0) {
            ++artnet;
            CHECK(packet.to.address == "10.0.0.5");
            CHECK(packet.to.port == 6454);
            CHECK(packet.bytes[14] == 1);  // Universe 1
        } else {
            ++sacn;
            CHECK(packet.to.port == 5568);
        }
    }
    CHECK(artnet == stats.output.framesProcessed);
    CHECK(sacn == stats.output.framesProcessed);
}

TEST_CASE("Session: ohne output-Zeile gibt es einen klaren Fehler", "[runtime]") {
    Rig rig;
    project::Project p = rig.load(kProject);
    p.target.reset();
    const auto r = rig.open(p);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("output") != std::string::npos);
}

TEST_CASE("Session: ein nicht übersetzbarer Graph scheitert beim Start", "[runtime]") {
    Rig rig;
    // Mirror ohne Quelle ergibt kein Bild.
    const project::Project p = rig.load(
        "whitehole-project 1\ndestination 0 10.0.0.5\nfixture A\n  matrix 2 2\n  patch artnet destination 0\nend\n"
        "node m wh.transform.mirror\nend\noutput m.out\n");
    const auto r = rig.open(p);
    REQUIRE_FALSE(r.ok());
}

TEST_CASE("Session: Fehler beim Gerät werden gemeldet, nichts bleibt hängen", "[runtime]") {
    Rig rig;
    const project::Project p = rig.load(kProject);
    rig.device.failNextBuffer();
    CHECK_FALSE(rig.open(p).ok());
}

TEST_CASE("Session: das Beenden wartet auf laufende Readbacks", "[runtime]") {
    Rig rig;
    const project::Project p = rig.load(kProject);
    {
        auto session = rig.open(p);
        REQUIRE(session.ok());
        rig.device.holdReadbacks(true);
        REQUIRE(session.value()->renderFrame().ok());
        rig.device.holdReadbacks(false);
    }  // Der Destruktor pollt die Rückrufe zu Ende, bevor er Ring und Worker abbaut.
    SUCCEED();
}

TEST_CASE("buildOutput: die sACN-Sender-ID hängt am Quellnamen und bleibt stabil", "[runtime]") {
    Rig rig;
    const std::string text = std::string{kProject};
    project::Project a = rig.load(text);
    project::Project b = rig.load(text);
    project::Project c = rig.load(text);
    c.sourceName = "Andere Quelle";
    RecordingSink s1;
    RecordingSink s2;
    RecordingSink s3;
    auto pa = runtime::buildOutput(a, s1);
    auto pb = runtime::buildOutput(b, s2);
    auto pc = runtime::buildOutput(c, s3);
    REQUIRE(pa.ok());
    REQUIRE(pb.ok());
    REQUIRE(pc.ok());
    const std::vector<std::byte> frame(108 * 16);
    REQUIRE(pa.value().pipeline->processBytes(frame).ok());
    REQUIRE(pb.value().pipeline->processBytes(frame).ok());
    REQUIRE(pc.value().pipeline->processBytes(frame).ok());
    // sACN-CID liegt ab Byte 22 des Pakets (nach Root-Layer-Präambel).
    const auto cidOf = [](const RecordingSink& s) {
        for (const auto& packet : s.packets()) {
            if (std::memcmp(packet.bytes.data(), "Art-Net", 7) != 0) {
                return std::vector<std::uint8_t>(packet.bytes.begin() + 22, packet.bytes.begin() + 38);
            }
        }
        return std::vector<std::uint8_t>{};
    };
    CHECK(cidOf(s1) == cidOf(s2));
    CHECK(cidOf(s1) != cidOf(s3));
    CHECK(cidOf(s1).size() == 16);
}

TEST_CASE("Session: die Farben des letzten Frames lassen sich abgreifen", "[runtime]") {
    Rig rig;
    const project::Project p = rig.load(kProject);
    {
        auto plain = rig.open(p);
        REQUIRE(plain.ok());
        REQUIRE(plain.value()->renderFrame().ok());
        REQUIRE(plain.value()->flush(std::chrono::seconds{5}));
        CHECK(plain.value()->latestColors().empty());  // nicht angefordert
    }
    runtime::SessionOptions options;
    options.keepLatestColors = true;
    auto session = runtime::Session::create(
        p, runtime::SessionDeps{rig.device, rig.library->snippets, rig.library->kernels, rig.sink, rig.clock}, options);
    REQUIRE(session.ok());
    CHECK(session.value()->latestColors().empty());  // noch kein Frame
    CHECK(session.value()->map().world.size() == 108);
    REQUIRE(session.value()->renderFrame().ok());
    REQUIRE(session.value()->flush(std::chrono::seconds{5}));
    const std::vector<ColorRgba> colors = session.value()->latestColors();
    CHECK(colors.size() == 108);
    // Das Fake-Gerät rechnet nicht: Nullen. Wichtig ist, dass der Frame hier ankommt und
    // trotzdem auch zur Ausgabe geht.
    CHECK(session.value()->stats().output.framesProcessed == 1);
}
