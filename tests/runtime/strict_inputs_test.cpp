// Tests für Issue #25: Ein unverbundener Eingang eines Blend-Nodes gilt als durchsichtiges Schwarz; der strenge Modus
// (settings ... strict-inputs on) stellt die Pflicht-Eingänge wieder her. Prüft Kompilieren, Datei und Player.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "builtin_lab.hpp"
#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::test::Lab;
using whitehole::testing::RecordingSink;

namespace {

// Blend mit nur Eingang A: B fehlt.
const char* const kHalfBlend = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node g wh.generator.plasma
end
node b wh.blend.blend
end
connect g.out -> b.a
output b.out
)";

std::string strict(const std::string& text) {
    std::string copy = text;
    copy.insert(copy.find("destination"), "settings strict-inputs on\n\n");
    return copy;
}

}  // namespace

TEST_CASE("Strikte Eingänge: Blend ohne B kompiliert nachsichtig, streng nicht", "[nodes][strict]") {
    Lab lab;
    const NodeId gen = lab.add("wh.generator.plasma");
    const NodeId blend = lab.add("wh.blend.blend");
    REQUIRE(lab.graph.connect(graph::PortRef{gen, "out"}, graph::PortRef{blend, "a"}).ok());
    const graph::PortRef out{blend, "out"};

    CHECK_FALSE(lab.graph.strictInputs());
    const auto lenient = compiler::compileField(lab.graph, lab.snippets, out);
    REQUIRE(lenient.ok());
    CHECK(lenient.value().wgsl.find("vec4<f32>(0.0, 0.0, 0.0, 0.0)") != std::string::npos);

    lab.graph.setStrictInputs(true);
    const auto harsh = compiler::compileField(lab.graph, lab.snippets, out);
    REQUIRE_FALSE(harsh.ok());
    CHECK(harsh.error().message.find("Pflicht-Eingang") != std::string::npos);
}

TEST_CASE("Strikte Eingänge: Nodes mit nur einem Eingang bleiben streng", "[nodes][strict]") {
    Lab lab;
    const NodeId adjust = lab.add("wh.modifier.coloradjust");
    CHECK_FALSE(compiler::compileField(lab.graph, lab.snippets, graph::PortRef{adjust, "out"}).ok());
    const NodeId key = lab.add("wh.blend.key");
    CHECK_FALSE(compiler::compileField(lab.graph, lab.snippets, graph::PortRef{key, "out"}).ok());
}

TEST_CASE("Strikte Eingänge: die Einstellung steht in der Datei und übersteht Schreiben und Lesen", "[project][strict]") {
    auto library = nodes::BuiltinLibrary::create().value();
    auto lenient = project::parseProject(kHalfBlend, library->registry);
    REQUIRE(lenient.ok());
    CHECK_FALSE(lenient.value().strictInputs);
    CHECK_FALSE(lenient.value().graph.strictInputs());
    const auto plain = project::writeProject(lenient.value());
    REQUIRE(plain.ok());
    CHECK(plain.value().find("strict-inputs") == std::string::npos);

    auto harsh = project::parseProject(strict(kHalfBlend), library->registry);
    REQUIRE(harsh.ok());
    CHECK(harsh.value().strictInputs);
    CHECK(harsh.value().graph.strictInputs());
    const auto written = project::writeProject(harsh.value());
    REQUIRE(written.ok());
    CHECK(written.value().find("strict-inputs on") != std::string::npos);
    auto again = project::parseProject(written.value(), library->registry);
    REQUIRE(again.ok());
    CHECK(again.value().strictInputs);

    std::string bad = strict(kHalfBlend);
    bad.replace(bad.find("strict-inputs on") + 14, 2, "ja");
    CHECK_FALSE(project::parseProject(bad, library->registry).ok());
}

TEST_CASE("Strikte Eingänge: der Player zeigt den Blend nachsichtig und schaltet streng um (mit Rückgängig)",
          "[player][strict]") {
    auto library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    REQUIRE(player.loadText(kHalfBlend).ok());
    CHECK_FALSE(player.strictInputs());
    CHECK_FALSE(player.outputIncomplete());

    REQUIRE(player.setStrictInputs(true).ok());
    CHECK(player.strictInputs());
    CHECK(player.outputIncomplete());  // B fehlt: LEDs aus
    CHECK(player.modified());

    REQUIRE(player.undo().ok());
    CHECK_FALSE(player.strictInputs());
    CHECK_FALSE(player.outputIncomplete());

    CHECK(player.setStrictInputs(false).ok());  // schon aus: nichts zu tun
    REQUIRE(player.start().ok());
    CHECK_FALSE(player.startLive(0).ok());  // keine Looks: Live geht nicht
}
