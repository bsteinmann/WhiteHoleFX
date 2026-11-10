// Tests der Live-Anzeige-Daten: ValueStore -> LiveSignal, Balkenbereich (LiveRange) und der Weg über den Player.
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <limits>
#include <thread>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/live_signals.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const runtime::LiveSignal* find(const std::vector<runtime::LiveSignal>& signals, NodeId node, const char* key) {
    for (const auto& signal : signals) {
        if (signal.node == node && signal.portKey == key) {
            return &signal;
        }
    }
    return nullptr;
}

}  // namespace

TEST_CASE("LiveSignals: Zahlen, Farben und Spektren aus dem ValueStore", "[runtime][live]") {
    values::ValueStore store;
    store.set(NodeId{1}, "level", 0.25);
    store.set(NodeId{1}, "on", true);
    store.set(NodeId{2}, "count", std::int64_t{7});
    store.set(NodeId{2}, "tint", ColorRgba{1.0F, 0.5F, 0.0F, 1.0F});
    store.set(NodeId{3}, "text", std::string{"hallo"});  // Texte haben keinen Pegel
    Spectrum spectrum;
    spectrum.bands[3] = 0.75F;
    store.setSpectrum(NodeId{4}, "bands", spectrum);

    const auto signals = runtime::collectLiveSignals(store);
    CHECK(signals.size() == 5);
    const auto* level = find(signals, NodeId{1}, "level");
    REQUIRE(level != nullptr);
    CHECK(level->kind == runtime::LiveSignalKind::Number);
    CHECK(level->number == 0.25);
    CHECK(find(signals, NodeId{1}, "on")->number == 1.0);
    CHECK(find(signals, NodeId{2}, "count")->number == 7.0);
    const auto* tint = find(signals, NodeId{2}, "tint");
    REQUIRE(tint != nullptr);
    CHECK(tint->kind == runtime::LiveSignalKind::Color);
    CHECK(tint->color.g == 0.5F);
    const auto* bands = find(signals, NodeId{4}, "bands");
    REQUIRE(bands != nullptr);
    CHECK(bands->kind == runtime::LiveSignalKind::Bars);
    CHECK(bands->bars.bands[3] == 0.75F);
    CHECK(find(signals, NodeId{3}, "text") == nullptr);
}

TEST_CASE("LiveRange: 0..1 zeigt den Wert, größere Werte weiten den Bereich auf", "[runtime][live]") {
    runtime::LiveRange range;
    CHECK(range.update(0.0) == 0.0F);
    CHECK(range.update(0.5) == 0.5F);
    CHECK(range.update(1.0) == 1.0F);

    // Wert 4: der Bereich wächst auf 0..4, 4 steht ganz oben, 2 in der Mitte.
    CHECK(range.update(4.0) == 1.0F);
    CHECK(range.update(2.0) > 0.45F);
    CHECK(range.update(2.0) < 0.55F);

    // Negative Werte öffnen den Bereich nach unten.
    runtime::LiveRange bipolar;
    CHECK(bipolar.update(-1.0) == 0.0F);
    CHECK(bipolar.update(1.0) == 1.0F);
    CHECK(bipolar.update(0.0) > 0.45F);
    CHECK(bipolar.update(0.0) < 0.55F);

    // Nach langer Ruhe fällt der Bereich zurück auf 0..1.
    for (int i = 0; i < 3000; ++i) {
        (void)range.update(0.5);
    }
    CHECK(range.high() < 1.01);
    CHECK(range.low() > -0.01);

    // Ungültiges bleibt ruhig.
    CHECK(runtime::LiveRange{}.update(std::numeric_limits<double>::quiet_NaN()) == 0.0F);
}

TEST_CASE("Player: liveSignals liefert die Ausgänge der CPU-Nodes", "[runtime][live]") {
    auto library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    REQUIRE(player.loadText(R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node lfo wh.value.lfo
  set frequency 1
end
node plasma wh.generator.plasma
end
connect lfo.out -> plasma.speed
output plasma.out
)").ok());

    CHECK(player.liveSignals().empty());  // ohne laufende Session nichts

    REQUIRE(player.start().ok());
    NodeId lfo;
    for (const auto& node : player.project()->graph.nodes()) {
        if (node.typeId == "wh.value.lfo") {
            lfo = node.id;
        }
    }
    const runtime::LiveSignal* found = nullptr;
    std::vector<runtime::LiveSignal> signals;
    for (int i = 0; i < 6 && found == nullptr; ++i) {
        REQUIRE(player.step().ok());
        std::this_thread::sleep_for(std::chrono::milliseconds{2});
        signals = player.liveSignals();
        found = find(signals, lfo, "out");
    }
    REQUIRE(found != nullptr);
    CHECK(found->kind == runtime::LiveSignalKind::Number);
    CHECK(found->number >= 0.0);
    CHECK(found->number <= 1.0);
}
