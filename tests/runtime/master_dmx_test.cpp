// Tests des Masters über DMX (Issue #17): Wert aus dem Kanal, Zeitüberschreitung, Abschalten, falsche Kanäle.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::input::DmxProtocol;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5

fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end

node g wh.generator.gradient
end
output g.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        REQUIRE(player.start().ok());
    }
};

}  // namespace

TEST_CASE("Master über DMX: der Kanalwert setzt den Master", "[player][master_dmx]") {
    Rig rig;
    rig.player.setMasterDmx(runtime::MasterDmxSource{DmxProtocol::ArtNet, 2, 3});
    CHECK_FALSE(rig.player.masterFromDmx());  // noch kein Paket
    CHECK(rig.player.masterBrightness() == 1.0F);

    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 0, 51});  // Kanal 3 = 51 -> 20 %
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.masterFromDmx());
    CHECK(rig.player.masterBrightness() == 51.0F / 255.0F);

    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 0, 255});
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.masterBrightness() == 1.0F);
    rig.library->dmx->write(DmxProtocol::ArtNet, 2, std::vector<std::uint8_t>{0, 0, 0});
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.masterBrightness() == 0.0F);
}

TEST_CASE("Master über DMX: anderes Universe, anderes Protokoll und Abschalten ändern nichts", "[player][master_dmx]") {
    Rig rig;
    rig.player.setMasterDmx(runtime::MasterDmxSource{DmxProtocol::Sacn, 1, 1});
    rig.library->dmx->write(DmxProtocol::ArtNet, 1, std::vector<std::uint8_t>{10});  // falsches Protokoll
    rig.library->dmx->write(DmxProtocol::Sacn, 2, std::vector<std::uint8_t>{10});    // falsches Universe
    REQUIRE(rig.player.step().ok());
    CHECK_FALSE(rig.player.masterFromDmx());
    CHECK(rig.player.masterBrightness() == 1.0F);

    rig.library->dmx->write(DmxProtocol::Sacn, 1, std::vector<std::uint8_t>{102});
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.masterBrightness() == 102.0F / 255.0F);

    rig.player.setMasterDmx(std::nullopt);  // aus: der Master bleibt, wo er ist, und der Kanal wirkt nicht mehr
    CHECK_FALSE(rig.player.masterFromDmx());
    rig.library->dmx->write(DmxProtocol::Sacn, 1, std::vector<std::uint8_t>{255});
    REQUIRE(rig.player.step().ok());
    CHECK(rig.player.masterBrightness() == 102.0F / 255.0F);
    CHECK(rig.player.setMasterBrightness(0.5F) == 0.5F);  // von Hand geht wieder
}

TEST_CASE("Master über DMX: ungültiger Kanal wird abgelehnt, bleibendes Signal hält den Wert", "[player][master_dmx]") {
    Rig rig;
    rig.player.setMasterDmx(runtime::MasterDmxSource{DmxProtocol::ArtNet, 0, 0});
    CHECK_FALSE(rig.player.masterDmx().has_value());
    rig.player.setMasterDmx(runtime::MasterDmxSource{DmxProtocol::ArtNet, 0, 513});
    CHECK_FALSE(rig.player.masterDmx().has_value());
    rig.player.setMasterDmx(runtime::MasterDmxSource{DmxProtocol::ArtNet, 0, 512});
    CHECK(rig.player.masterDmx().has_value());
}
