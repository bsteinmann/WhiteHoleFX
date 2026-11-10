// onset_node_test.cpp: der Node "Audio Trigger" (wh.value.onset; früher "Kick / Snare / Hat") mit von Hand gefülltem AudioStore.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <variant>

#include "builtin_lab.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;

namespace {

audio::AudioFrame quietFrame() {
    audio::AudioFrame frame;
    frame.rms = 0.1F;
    frame.frameCount = 1;
    return frame;
}

bool hitFlag(Lab& lab, NodeId node) {
    const auto value = lab.store.value(node, "beat");
    REQUIRE(value.has_value());
    return std::get<bool>(*value);
}

}  // namespace

TEST_CASE("Onset-Node: Kick löst aus, Puls klingt ab, Snare wird ignoriert", "[audio][nodes][onset]") {
    Lab lab;
    const NodeId node = lab.add("wh.value.onset");
    lab.feed(node, "pulse");
    lab.set(node, "decay", 0.5);

    audio::AudioFrame frame = quietFrame();
    frame.kick.count = 3;  // alte Einsätze vor dem ersten Blick zählen nicht
    frame.kick.strength = 2.0F;
    lab.audio->write(frame);
    lab.run(0.0, 0.016);
    CHECK_FALSE(hitFlag(lab, node));

    frame.snare.count = 1;  // anderer Bereich: kein Treffer
    frame.snare.strength = 3.0F;
    lab.audio->write(frame);
    lab.run(0.5, 0.016);
    CHECK_FALSE(hitFlag(lab, node));

    frame.kick.count = 4;
    lab.audio->write(frame);
    lab.run(1.0, 0.016);
    CHECK(hitFlag(lab, node));
    CHECK(lab.number(node, "pulse") == Approx(1.0));
    CHECK(lab.number(node, "strength") > 0.5);

    lab.run(1.25, 0.25);
    CHECK_FALSE(hitFlag(lab, node));
    CHECK(lab.number(node, "pulse") == Approx(std::exp(-0.5)).margin(0.001));
    CHECK(lab.number(node, "age") == Approx(0.25).margin(0.001));  // Sekunden seit dem Treffer
}

TEST_CASE("Onset-Node: Instrument wählbar, Mindeststärke filtert schwache Einsätze", "[audio][nodes][onset]") {
    Lab lab;
    const NodeId node = lab.add("wh.value.onset");
    lab.feed(node, "pulse");
    lab.set(node, "band", std::int64_t{2});  // Hi-Hat

    audio::AudioFrame frame = quietFrame();
    lab.audio->write(frame);
    lab.run(0.0, 0.016);

    frame.hat.count = 1;
    frame.hat.strength = 1.5F;
    lab.audio->write(frame);
    lab.run(0.1, 0.016);
    CHECK(hitFlag(lab, node));

    lab.set(node, "threshold", 3.0);
    frame.hat.count = 2;
    lab.audio->write(frame);
    lab.run(0.2, 0.016);
    CHECK_FALSE(hitFlag(lab, node));  // 1,5 liegt unter der Mindeststärke 3
}

TEST_CASE("Onset-Node: ohne Signal kein Treffer und present falsch", "[audio][nodes][onset]") {
    Lab lab;
    const NodeId node = lab.add("wh.value.onset");
    lab.feed(node, "pulse");
    lab.run(0.0, 0.016);
    CHECK_FALSE(hitFlag(lab, node));
    const auto present = lab.store.value(node, "present");
    REQUIRE(present.has_value());
    CHECK_FALSE(std::get<bool>(*present));
}

TEST_CASE("Onset-Node: eigener Bereich (From/To Hz) löst nur im gewählten Bereich aus", "[audio][nodes][onset]") {
    Lab lab;
    const NodeId node = lab.add("wh.value.onset");
    lab.feed(node, "pulse");
    lab.set(node, "band", std::int64_t{3});
    lab.set(node, "from", 800.0);
    lab.set(node, "to", 1500.0);

    audio::AudioFrame frame = quietFrame();
    frame.frameCount = 0;
    double time = 0.0;
    const auto step = [&](std::size_t first, std::size_t last, float level) {
        frame.frameCount += 1;
        frame.audioTime += 0.02;
        time += 0.02;
        for (std::size_t i = first; i <= last; ++i) {
            frame.spectrum.bands[i] = level;
        }
        lab.audio->write(frame);
        lab.run(time, 0.02);
    };
    // Welche Bänder der Node meint, sagt er selbst (derselbe Bereich wie im Anzeigestreifen)
    step(0, kSpectrumBands - 1, 0.1F);
    const auto monitor = lab.store.allMonitors().find(node);
    REQUIRE(monitor != lab.store.allMonitors().end());
    const auto first = static_cast<std::size_t>(monitor->second.rangeFirst);
    const auto last = static_cast<std::size_t>(monitor->second.rangeLast);
    REQUIRE(first <= last);
    REQUIRE(last < kSpectrumBands - 1);
    CHECK(first > 8);  // klar oberhalb des Kick-Bereichs

    for (int i = 0; i < 30; ++i) {
        step(0, kSpectrumBands - 1, 0.1F);
        CHECK_FALSE(hitFlag(lab, node));
    }
    // Ein Sprung außerhalb des Bereichs zählt nicht, auch nicht der Kick-Zähler des Analysators
    frame.kick.count = 5;
    frame.kick.strength = 3.0F;
    step(last + 1, kSpectrumBands - 1, 0.9F);
    CHECK_FALSE(hitFlag(lab, node));
    step(0, first - 1, 0.9F);
    CHECK_FALSE(hitFlag(lab, node));

    // Ein Sprung im Bereich löst aus
    step(first, last, 0.7F);
    CHECK(hitFlag(lab, node));
    CHECK(lab.number(node, "pulse") == Approx(1.0));
    for (int i = 0; i < 3; ++i) {
        step(first, last, 0.7F);  // bleibt oben: kein weiterer Treffer
        CHECK_FALSE(hitFlag(lab, node));
    }
    // Abfall und ein neuer Anstieg nach genug Pause löst wieder aus
    for (int i = 0; i < 8; ++i) {
        step(first, last, 0.1F);
        CHECK_FALSE(hitFlag(lab, node));
    }
    step(first, last, 0.7F);
    CHECK(hitFlag(lab, node));
}

TEST_CASE("Onset-Node: Anzeigebereich folgt dem eigenen Bereich", "[audio][nodes][onset][monitor]") {
    Lab lab;
    const NodeId node = lab.add("wh.value.onset");
    lab.feed(node, "pulse");
    lab.set(node, "band", std::int64_t{3});
    lab.set(node, "from", 3000.0);
    lab.set(node, "to", 100.0);  // verdreht: wird sortiert
    lab.run(0.0, 0.016);
    const auto monitor = lab.store.allMonitors().find(node);
    REQUIRE(monitor != lab.store.allMonitors().end());
    CHECK(monitor->second.rangeFirst < 8);
    CHECK(monitor->second.rangeLast > 20);
    CHECK(monitor->second.rangeFirst <= monitor->second.rangeLast);
}
