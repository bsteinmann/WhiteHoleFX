// audio_nodes_test.cpp: die Audio-Nodes (Pegel, Band, Spektrum, Beat, Tap) und der Weg des Spektrums
// bis in den Parameterpuffer des Shaders. Der AudioStore wird von Hand gefüllt, ohne Gerät.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <variant>

#include "builtin_lab.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;
using whitehole::test::slotFor;
using namespace std::chrono_literals;

namespace {

audio::AudioFrame frameWith(float rms, float peak = 0.0F) {
    audio::AudioFrame frame;
    frame.rms = rms;
    frame.peak = peak;
    frame.frameCount = 1;
    return frame;
}

bool flag(Lab& lab, NodeId node, const char* port) {
    const auto value = lab.store.value(node, port);
    REQUIRE(value.has_value());
    return std::get<bool>(*value);
}

/// Hängt alle Ausgänge an Verbraucher, damit der Evaluator den Node überhaupt auswertet.
NodeId watched(Lab& lab, const char* type, const char* port) {
    const NodeId node = lab.add(type);
    lab.feed(node, port);
    return node;
}

}  // namespace

TEST_CASE("Audio Level: dB-Skala, linear, Verstärkung", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.audiolevel", "level");
    lab.set(node, "attack", 0.0);
    lab.audio->write(frameWith(0.1F, 0.5F));  // 0,1 = -20 dB

    CHECK(lab.sample(node, 0.0, "level") == Approx(40.0 / 60.0).margin(0.001));
    CHECK(lab.number(node, "peak") == Approx((20.0 * std::log10(0.5) + 60.0) / 60.0).margin(0.001));
    CHECK(flag(lab, node, "present"));

    lab.set(node, "scale", std::int64_t{0});  // linear
    CHECK(lab.sample(node, 0.1, "level") == Approx(0.1).margin(0.001));
    lab.set(node, "gain", 3.0);
    CHECK(lab.sample(node, 0.2, "level") == Approx(0.3).margin(0.001));
    lab.set(node, "gain", 100.0);
    CHECK(lab.sample(node, 0.3, "level") == 1.0);  // begrenzt
}

TEST_CASE("Audio Level: ohne Signal Stille, Release lässt den Wert auslaufen", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.audiolevel", "level");
    lab.set(node, "scale", std::int64_t{0});
    lab.set(node, "attack", 0.0);
    lab.set(node, "release", 1.0);

    CHECK(lab.sample(node, 0.0, "level") == 0.0);  // noch nie ein Frame
    CHECK_FALSE(flag(lab, node, "present"));

    lab.audio->write(frameWith(0.8F));
    CHECK(lab.sample(node, 0.1, "level") == Approx(0.8).margin(0.001));

    lab.audioNow += 2s;  // Frame ist jetzt älter als eine halbe Sekunde: Signal weg
    lab.run(1.1, 1.0);
    CHECK_FALSE(flag(lab, node, "present"));
    CHECK(lab.number(node, "level") == Approx(0.8 * std::exp(-1.0)).margin(0.01));
}

TEST_CASE("Audio Level: Auto-Gain skaliert leise Signale auf volle Höhe", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.audiolevel", "level");
    lab.set(node, "scale", std::int64_t{0});
    lab.set(node, "attack", 0.0);
    lab.set(node, "auto", true);
    lab.audio->write(frameWith(0.2F));
    CHECK(lab.sample(node, 0.0, "level") == Approx(1.0).margin(0.001));
}

TEST_CASE("Audio Band: größter Wert der Bänder im Bereich, Verstärkung, Rauschgrenze", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.audioband", "value");
    lab.set(node, "attack", 0.0);

    audio::AudioFrame frame = frameWith(0.1F);
    const auto& edges = audio::bandEdgesHz();
    // Band 10 ist laut, Band 20 mittel, alles andere still.
    frame.spectrum.bands[10] = 0.8F;
    frame.spectrum.bands[20] = 0.4F;
    lab.audio->write(frame);

    const double inBand10 = (edges[10] + edges[11]) / 2.0;
    const double inBand20 = (edges[20] + edges[21]) / 2.0;

    lab.set(node, "from", inBand10);
    lab.set(node, "to", inBand10);
    CHECK(lab.sample(node, 0.0, "value") == Approx(0.8).margin(0.001));

    lab.set(node, "from", inBand20);
    lab.set(node, "to", inBand20);
    CHECK(lab.sample(node, 0.1, "value") == Approx(0.4).margin(0.001));

    lab.set(node, "from", inBand10);  // Bereich über beide
    lab.set(node, "to", inBand20);
    CHECK(lab.sample(node, 0.2, "value") == Approx(0.8).margin(0.001));

    lab.set(node, "from", inBand20);
    lab.set(node, "to", inBand20);
    lab.set(node, "gain", 2.0);
    CHECK(lab.sample(node, 0.3, "value") == Approx(0.8).margin(0.001));
    lab.set(node, "gain", 1.0);
    lab.set(node, "floor", 0.4);  // alles bis 0,4 gilt als Rauschen
    CHECK(lab.sample(node, 0.4, "value") == Approx(0.0).margin(0.001));
}

TEST_CASE("Audio Band: vertauschte Grenzen und Bereich zwischen zwei Bandgrenzen funktionieren", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.audioband", "value");
    lab.set(node, "attack", 0.0);
    audio::AudioFrame frame = frameWith(0.1F);
    frame.spectrum.bands[0] = 0.7F;
    lab.audio->write(frame);
    lab.set(node, "from", 60.0);  // "from" größer als "to": wird getauscht
    lab.set(node, "to", 40.0);
    CHECK(lab.sample(node, 0.0, "value") == Approx(0.7).margin(0.001));
}

TEST_CASE("Beat: Trigger nur bei neuem Schlag, Puls klingt ab, Stärke-Schwelle filtert", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.beat", "pulse");
    lab.set(node, "decay", 0.5);

    audio::AudioFrame frame = frameWith(0.1F);
    frame.beat.count = 5;  // alte Schläge vor dem ersten Blick zählen nicht
    frame.beat.strength = 2.0F;
    frame.beat.bpm = 120.0F;
    lab.audio->write(frame);
    lab.run(0.0, 0.016);
    CHECK_FALSE(flag(lab, node, "beat"));
    CHECK(lab.number(node, "bpm") == Approx(120.0));

    frame.beat.count = 6;
    lab.audio->write(frame);
    lab.run(1.0, 0.016);
    CHECK(flag(lab, node, "beat"));
    CHECK(lab.number(node, "pulse") == Approx(1.0));
    CHECK(lab.number(node, "phase") == Approx(0.0).margin(0.001));

    lab.run(1.25, 0.25);  // ein Viertelsekunde später: kein neuer Schlag
    CHECK_FALSE(flag(lab, node, "beat"));
    CHECK(lab.number(node, "pulse") == Approx(std::exp(-0.5)).margin(0.001));
    CHECK(lab.number(node, "phase") == Approx(0.5).margin(0.001));  // 120 BPM: halbe Periode

    lab.set(node, "threshold", 3.0);  // der nächste Schlag hat Stärke 2: zu schwach
    frame.beat.count = 7;
    lab.audio->write(frame);
    lab.run(1.5, 0.25);
    CHECK_FALSE(flag(lab, node, "beat"));

    lab.set(node, "threshold", 1.0);
    frame.beat.count = 8;
    lab.audio->write(frame);
    lab.run(1.6, 0.1);
    CHECK(flag(lab, node, "beat"));
}

TEST_CASE("Beat: festes Tempo überstimmt die Erkennung; ohne Tempo bleibt die Phase 0", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.beat", "phase");
    audio::AudioFrame frame = frameWith(0.1F);
    frame.beat.bpm = 0.0F;
    lab.audio->write(frame);
    CHECK(lab.sample(node, 0.0, "phase") == 0.0);

    lab.set(node, "manualBpm", 60.0);
    frame.beat.count = 1;
    frame.beat.strength = 1.5F;
    lab.audio->write(frame);
    lab.run(1.0, 0.016);  // erster Blick mit Zählerstand 1: kein Schlag
    lab.run(1.25, 0.25);
    CHECK(lab.number(node, "bpm") == 60.0);
}

TEST_CASE("Tap Tempo: Tippen ergibt Tempo und Takt, lange Pause beginnt neu", "[audio][nodes]") {
    Lab lab;
    const NodeId node = watched(lab, "wh.value.tap", "phase");
    const auto tap = [&](double at) {
        lab.set(node, "tap", true);
        lab.run(at, 0.01);
        lab.set(node, "tap", false);
        lab.run(at + 0.05, 0.05);
    };

    lab.run(0.0, 0.01);
    CHECK(lab.number(node, "bpm") == 0.0);
    tap(1.0);
    CHECK(lab.number(node, "bpm") == 0.0);  // ein Tipp allein gibt noch kein Tempo
    tap(1.5);
    tap(2.0);
    tap(2.5);
    CHECK(lab.number(node, "bpm") == Approx(120.0).margin(0.5));

    // Nach dem letzten Tipp läuft der Takt weiter: bei t = 3.0 fällt der nächste Schlag.
    lab.run(2.9, 0.1);
    CHECK_FALSE(flag(lab, node, "beat"));
    lab.run(3.01, 0.11);
    CHECK(flag(lab, node, "beat"));
    CHECK(lab.number(node, "phase") == Approx(0.02).margin(0.03));

    tap(10.0);  // mehr als 2 s Pause: neue Folge, Tempo bleibt zunächst das alte
    tap(10.8);
    CHECK(lab.number(node, "bpm") == Approx(60.0 / 0.8).margin(0.5));

    lab.set(node, "manualBpm", 90.0);
    lab.run(11.0, 0.1);
    CHECK(lab.number(node, "bpm") == 90.0);
}

TEST_CASE("Audio Spectrum: Ausgang liefert das Spektrum, geglättet und mit Verstärkung", "[audio][nodes]") {
    Lab lab;
    const NodeId node = lab.add("wh.value.audiospectrum");
    const NodeId bars = lab.add("wh.generator.spectrum");
    lab.connect(node, "spectrum", bars, "spectrum");
    lab.set(node, "attack", 0.0);

    audio::AudioFrame frame = frameWith(0.1F);
    for (std::size_t i = 0; i < kSpectrumBands; ++i) {
        frame.spectrum.bands[i] = static_cast<float>(i) / 40.0F;
    }
    lab.audio->write(frame);
    lab.run(0.0, 0.016);
    const auto spectrum = lab.store.spectrum(node, "spectrum");
    REQUIRE(spectrum.has_value());
    CHECK(spectrum->bands[0] == 0.0F);
    CHECK(spectrum->bands[20] == Approx(0.5F));
    CHECK(spectrum->bands[31] == Approx(31.0F / 40.0F));
    CHECK(flag(lab, node, "present"));

    lab.set(node, "gain", 2.0);
    lab.run(0.1, 0.016);
    CHECK(lab.store.spectrum(node, "spectrum")->bands[20] == Approx(1.0F));  // begrenzt
}

TEST_CASE("Spectrum Bars: Spektrum landet im Parameterpuffer, Shader indiziert es als vec4-Feld", "[audio][nodes]") {
    Lab lab;
    const NodeId spectrumNode = lab.add("wh.value.audiospectrum");
    const NodeId bars = lab.add("wh.generator.spectrum");
    lab.connect(spectrumNode, "spectrum", bars, "spectrum");
    lab.set(spectrumNode, "attack", 0.0);

    const auto compiled = compiler::compileField(lab.graph, lab.snippets, graph::PortRef{bars, "out"});
    REQUIRE(compiled.ok());
    const std::string& wgsl = compiled.value().wgsl;
    CHECK(wgsl.find("array<vec4<f32>, 8>") != std::string::npos);
    CHECK(wgsl.find("/ 4u]") != std::string::npos);
    CHECK(wgsl.find("@spectrum") == std::string::npos);  // Platzhalter vollständig ersetzt

    const compiler::ParameterSlot* slot = slotFor(compiled.value().layout, bars, "spectrum");
    REQUIRE(slot != nullptr);
    CHECK(slot->origin == compiler::SlotOrigin::SpectrumInput);
    CHECK(slot->type == compiler::GpuType::Spectrum);
    CHECK(slot->offsetBytes % 16 == 0);

    audio::AudioFrame frame = frameWith(0.1F);
    frame.spectrum.bands[3] = 0.25F;
    frame.spectrum.bands[31] = 0.75F;
    lab.audio->write(frame);
    lab.run(0.0, 0.016);
    const auto bytes = compiler::packParameters(lab.graph, compiled.value().layout, &lab.store);
    CHECK(test::floatAt(bytes, slot->offsetBytes + 3 * 4) == 0.25F);
    CHECK(test::floatAt(bytes, slot->offsetBytes + 31 * 4) == 0.75F);
    CHECK(test::floatAt(bytes, slot->offsetBytes + 4 * 4) == 0.0F);

    // Ohne Werte (Spektrum nicht ausgewertet) bleibt der Slot null: Stille.
    const auto silent = compiler::packParameters(lab.graph, compiled.value().layout);
    CHECK(test::floatAt(silent, slot->offsetBytes + 3 * 4) == 0.0F);
}

TEST_CASE("Spectrum Bars: ohne Verbindung kompiliert der Node und sieht Stille", "[audio][nodes]") {
    Lab lab;
    const NodeId bars = lab.add("wh.generator.spectrum");
    const auto compiled = compiler::compileField(lab.graph, lab.snippets, graph::PortRef{bars, "out"});
    REQUIRE(compiled.ok());
    const compiler::ParameterSlot* slot = slotFor(compiled.value().layout, bars, "spectrum");
    REQUIRE(slot != nullptr);
    const auto bytes = compiler::packParameters(lab.graph, compiled.value().layout);
    for (std::size_t i = 0; i < kSpectrumBands; ++i) {
        CHECK(test::floatAt(bytes, slot->offsetBytes + static_cast<std::uint32_t>(i * 4)) == 0.0F);
    }
}

TEST_CASE("Spektrum-Ausgang lässt sich nur mit Spektrum-Eingängen verbinden", "[audio][nodes]") {
    Lab lab;
    const NodeId spectrumNode = lab.add("wh.value.audiospectrum");
    const NodeId rotate = lab.add("wh.transform.rotate");
    CHECK_FALSE(lab.graph.connect({spectrumNode, "spectrum"}, {rotate, "angle"}).ok());
    const NodeId level = lab.add("wh.value.audiolevel");
    const NodeId bars = lab.add("wh.generator.spectrum");
    CHECK_FALSE(lab.graph.connect({level, "level"}, {bars, "spectrum"}).ok());
}

TEST_CASE("Beispiel: examples/audio.whfx lädt, kompiliert und das Spektrum erreicht den Puffer", "[audio][nodes]") {
    const auto path = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples" / "audio.whfx";
    const auto text = project::readTextFile(path.string());
    REQUIRE(text.ok());
    Lab lab;
    auto parsed = project::parseProject(text.value(), lab.registry);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().target.has_value());
    const auto compiled = compiler::compileField(parsed.value().graph, lab.snippets, *parsed.value().target);
    REQUIRE(compiled.ok());
    CHECK(compiled.value().wgsl.find("array<vec4<f32>, 8>") != std::string::npos);
}
