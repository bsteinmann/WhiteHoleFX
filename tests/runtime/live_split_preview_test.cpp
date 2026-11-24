// Geteilte Vorschau im Player (Attrappen-Gerät): wann das verdeckte Deck mitgerechnet wird und welcher Look rechts steht.
// Die Farben selbst prüft tests/gpu_device/split_preview_on_device_test.cpp auf einem echten Gerät.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
end
output plasma.out

look Eins fade 1
node a wh.generator.plasma
end
output a.out
endlook

look Zwei fade 1
node b wh.generator.noise
end
output b.out
endlook
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    double clock{0.0};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        player.setLiveClock([this] { return clock; });
        REQUIRE(player.start().ok());
    }
    void run(double seconds) {
        clock += seconds;
        REQUIRE(player.step().ok());
    }
    [[nodiscard]] std::string lastShader() const {
        REQUIRE_FALSE(device.pipelineDescs().empty());
        return device.pipelineDescs().back().shader.code;
    }
};

}  // namespace

TEST_CASE("Geteilte Vorschau: aus kostet nichts, an rechnet das verdeckte Deck mit", "[live][secondary]") {
    Rig rig;
    REQUIRE(rig.player.startLive(0).ok());
    rig.run(0.0);
    CHECK_FALSE(rig.player.splitPreview());
    CHECK(rig.lastShader().find("wh_field_extra") == std::string::npos);
    REQUIRE(rig.player.liveArm(1).ok());
    rig.run(0.0);
    CHECK_FALSE(rig.player.previewLook().has_value());  // aus: keine rechte Hälfte

    rig.player.setSplitPreview(true);
    rig.run(0.0);  // baut die Live-Ausgabe einmal neu auf
    CHECK(rig.lastShader().find("if (wh_leds.extra < 2u)") != std::string::npos);
    CHECK(rig.player.previewLook() == 1);
    CHECK(rig.player.previewImage(preview::PreviewOptions{}).error().code == ErrorCode::NotFound);  // Attrappe rechnet nicht

    const std::size_t built = rig.device.pipelineDescs().size();
    REQUIRE(rig.player.liveGo(1.0).ok());
    rig.run(0.5);
    rig.run(0.5);
    CHECK(rig.player.liveStatus().fading);
    CHECK(rig.player.previewLook() == 1);  // während des Einblendens
    rig.run(0.6);
    rig.run(0.0);
    CHECK_FALSE(rig.player.previewLook().has_value());  // fertig: nichts mehr vorgeladen
    CHECK(rig.device.pipelineDescs().size() == built);  // Überblenden und Ende des Fades übersetzen nichts neu

    rig.player.stopLive();
    rig.run(0.0);
    CHECK_FALSE(rig.player.previewLook().has_value());
    CHECK(rig.lastShader().find("wh_field_extra") == std::string::npos);  // im Editor ohne Zusatzausgang
}

TEST_CASE("Geteilte Vorschau: schon beim Start des Live-Betriebs eingeschaltet", "[live][secondary]") {
    Rig rig;
    rig.player.setSplitPreview(true);
    REQUIRE(rig.player.startLive(0).ok());
    rig.run(0.0);
    CHECK_FALSE(rig.player.previewLook().has_value());
    CHECK(rig.lastShader().find("if (wh_leds.extra < 2u)") != std::string::npos);  // gleich beim ersten Aufbau
    const std::size_t built = rig.device.pipelineDescs().size();
    REQUIRE(rig.player.liveArm(1).ok());
    rig.run(0.0);
    CHECK(rig.player.previewLook() == 1);
    CHECK(rig.device.pipelineDescs().size() == built + 1);  // Vorladen übersetzt einmal, wie ohne Vorschau
}
