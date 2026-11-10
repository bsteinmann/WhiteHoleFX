// Anzeigestreifen der Audio-Nodes: Band und Kick/Snare/Hat melden das Spektrum samt ausgewertetem Bereich, Spectrum
// und Waveform ihre Kurve. Der AudioStore wird von Hand gefüllt, ohne Gerät.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "builtin_lab.hpp"
#include "whitehole/runtime/live_signals.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

namespace {

NodeId watched(Lab& lab, const char* type, const char* port) {
    const NodeId node = lab.add(type);
    lab.feed(node, port);
    return node;
}

audio::AudioFrame loudFrame() {
    audio::AudioFrame frame;
    frame.rms = 0.1F;
    frame.frameCount = 1;
    frame.spectrum.bands[2] = 0.9F;
    frame.spectrum.bands[12] = 0.5F;
    return frame;
}

const MonitorView& monitorOf(const Lab& lab, NodeId node) {
    const auto& all = lab.store.allMonitors();
    const auto found = all.find(node);
    REQUIRE(found != all.end());
    return found->second;
}

}  // namespace

TEST_CASE("Monitor: Audio Band zeigt das ganze Spektrum und markiert den Bereich", "[audio][nodes][monitor]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.audioband", "value");
    lab.audio->write(loudFrame());
    const auto& edges = audio::bandEdgesHz();
    lab.set(node, "from", (edges[10] + edges[11]) / 2.0);
    lab.set(node, "to", (edges[13] + edges[14]) / 2.0);
    lab.run(0.0, 0.0);

    const MonitorView& view = monitorOf(lab, node);
    CHECK(view.rangeFirst == 10);
    CHECK(view.rangeLast == 13);
    CHECK_FALSE(view.bipolar);
    CHECK(view.bands.bands[2] == Approx(0.9F));
    CHECK(view.bands.bands[12] == Approx(0.5F));

    // Verstärkung und Schwelle gehören zur Anzeige (so, wie sie in den Wert eingehen)
    lab.set(node, "gain", 2.0);
    lab.run(0.1, 0.1);
    CHECK(monitorOf(lab, node).bands.bands[12] == Approx(1.0F));
    lab.set(node, "gain", 1.0);
    lab.set(node, "floor", 0.6);
    lab.run(0.2, 0.1);
    CHECK(monitorOf(lab, node).bands.bands[12] == Approx(0.0F));
    CHECK(monitorOf(lab, node).bands.bands[2] > 0.5F);
}

TEST_CASE("Monitor: Audio Band ohne Signal zeigt den Bereich trotzdem, die Balken sind leer", "[audio][nodes][monitor]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.audioband", "value");
    lab.set(node, "from", 60.0);
    lab.set(node, "to", 250.0);
    lab.run(0.0, 0.0);
    const MonitorView& view = monitorOf(lab, node);
    CHECK(view.rangeFirst >= 0);
    CHECK(view.rangeLast >= view.rangeFirst);
    for (const float band : view.bands.bands) {
        CHECK(band == 0.0F);
    }
}

TEST_CASE("Monitor: Kick, Snare und Hi-Hat markieren ihre Bänder", "[audio][nodes][monitor]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.onset", "pulse");
    lab.audio->write(loudFrame());
    const auto check = [&](std::int64_t instrument, int first, int last) {
        lab.set(node, "band", instrument);
        lab.run(0.0, 0.0);
        CHECK(monitorOf(lab, node).rangeFirst == first);
        CHECK(monitorOf(lab, node).rangeLast == last);
    };
    check(0, static_cast<int>(audio::kKickFirstBand), static_cast<int>(audio::kKickEndBand) - 1);
    check(1, static_cast<int>(audio::kSnareFirstBand), static_cast<int>(audio::kSnareEndBand) - 1);
    check(2, static_cast<int>(audio::kHatFirstBand), static_cast<int>(kSpectrumBands) - 1);
    CHECK(monitorOf(lab, node).bands.bands[2] == Approx(0.9F));
}

TEST_CASE("Monitor: Spectrum und Waveform liefern ihre Kurve ohne Bereich", "[audio][nodes][monitor]") {
    Lab lab;
    const NodeId spectrum = watched(lab, "wh.value.audiospectrum", "present");
    const NodeId wave = watched(lab, "wh.value.audiowave", "present");
    lab.set(spectrum, "attack", 0.0);
    lab.audio->write(loudFrame());
    lab.run(0.0, 0.0);
    const MonitorView& s = monitorOf(lab, spectrum);
    CHECK(s.rangeFirst == -1);
    CHECK_FALSE(s.bipolar);
    CHECK(s.bands.bands[2] == Approx(0.9F));
    const MonitorView& w = monitorOf(lab, wave);
    CHECK(w.bipolar);
    CHECK(w.rangeFirst == -1);
}

TEST_CASE("Monitor: nur Nodes mit Anzeigestreifen sind im Descriptor markiert", "[audio][nodes][monitor]") {
    Lab lab;
    const auto liveView = [&](const char* type) {
        const NodeId node = lab.add(type);
        return lab.graph.findNode(node) != nullptr ? lab.registry->find(type)->liveView : graph::LiveView::None;
    };
    CHECK(liveView("wh.value.audioband") == graph::LiveView::Spectrum);
    CHECK(liveView("wh.value.audiospectrum") == graph::LiveView::Spectrum);
    CHECK(liveView("wh.value.onset") == graph::LiveView::Spectrum);
    CHECK(liveView("wh.value.audiowave") == graph::LiveView::Wave);
    CHECK(liveView("wh.value.audiolevel") == graph::LiveView::None);
    CHECK(liveView("wh.value.beat") == graph::LiveView::None);
    CHECK(liveView("wh.value.tap") == graph::LiveView::None);
}

TEST_CASE("Monitor: collectLiveSignals reicht den Streifen samt Bereich weiter", "[audio][nodes][monitor]") {
    values::ValueStore store;
    MonitorView view;
    view.bands.bands[5] = 0.5F;
    view.rangeFirst = 3;
    view.rangeLast = 7;
    store.setMonitor(NodeId{9}, view);
    const auto signals = runtime::collectLiveSignals(store);
    REQUIRE(signals.size() == 1);
    CHECK(signals[0].node == NodeId{9});
    CHECK(signals[0].portKey == runtime::kMonitorKey);
    CHECK(signals[0].kind == runtime::LiveSignalKind::Monitor);
    CHECK(signals[0].rangeFirst == 3);
    CHECK(signals[0].rangeLast == 7);
    CHECK(signals[0].bars.bands[5] == 0.5F);
}
