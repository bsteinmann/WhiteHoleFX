// audio_wave_test.cpp: die Samples am AudioFrame, der Node "Audio Waveform" (Fenster, Trigger,
// Glättung) und der Porttyp AudioWave auf dem Weg zum Oszilloskop-Feld.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <variant>

#include "builtin_lab.hpp"
#include "signal_support.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::Lab;
using whitehole::test::slotFor;

namespace {

/// Ein Frame mit einem 50-Hz-Sinus (eine Periode = 960 Samples bei 48 kHz) als jüngste Samples.
audio::AudioFrame sineFrame(double phase) {
    audio::AudioFrame frame;
    frame.sampleRate = 48000.0;
    frame.frameCount = 1;
    frame.recent.resize(audio::kRecentSamples);
    for (std::size_t i = 0; i < frame.recent.size(); ++i) {
        frame.recent[i] = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * 50.0 * static_cast<double>(i) / 48000.0 + phase));
    }
    return frame;
}

/// Audio Waveform mit Verbraucher (Oszilloskop), damit der Evaluator ihn auswertet.
NodeId waveWithScope(Lab& lab, NodeId* scope = nullptr) {
    const NodeId node = lab.add("wh.value.audiowave");
    const NodeId bar = lab.add("wh.generator.scope");
    lab.connect(node, "wave", bar, "wave");
    if (scope != nullptr) {
        *scope = bar;
    }
    return node;
}

Spectrum waveOf(Lab& lab, NodeId node) {
    const auto wave = lab.store.spectrum(node, "wave");
    REQUIRE(wave.has_value());
    return *wave;
}

}  // namespace

TEST_CASE("Analyzer: der Frame trägt die letzten Samples, älteste zuerst", "[audio][wave]") {
    audio::AudioAnalyzer analyzer;
    const std::vector<float> signal = test::sine(440.0, 1.0);
    REQUIRE(analyzer.push(signal) > 0);
    const audio::AudioFrame& frame = analyzer.frame();
    REQUIRE(frame.recent.size() == audio::kRecentSamples);
    CHECK(frame.sampleRate == 48000.0);
    // Der Frame entsteht nach einem vollen Schritt (512 Samples): das Ende der Samples liegt
    // höchstens einen Schritt vor dem Ende des Signals.
    const std::size_t totalAtStep = static_cast<std::size_t>(frame.audioTime * 48000.0 + 0.5);
    REQUIRE(totalAtStep <= signal.size());
    CHECK(frame.recent.back() == Approx(signal[totalAtStep - 1]).margin(1e-6));
    CHECK(frame.recent[frame.recent.size() - 100] == Approx(signal[totalAtStep - 100]).margin(1e-6));
}

TEST_CASE("Audio Waveform: ein Sinus ergibt eine Welle, der Trigger beginnt am Nulldurchgang", "[audio][wave]") {
    Lab lab;
    const NodeId node = waveWithScope(lab);
    lab.set(node, "span", 20.0);  // genau eine Periode von 50 Hz auf 32 Punkte
    lab.audio->write(sineFrame(1.0));  // Phase 1 rad: ohne Trigger beginnt das Fenster mitten in der Welle
    lab.run(0.0, 0.016);
    const Spectrum triggered = waveOf(lab, node);
    CHECK(triggered.bands[0] >= 0.0F);
    CHECK(triggered.bands[0] < 0.3F);
    CHECK(triggered.bands[8] > 0.95F);    // Viertelperiode: Maximum
    CHECK(triggered.bands[24] < -0.95F);  // Dreiviertel: Minimum

    lab.set(node, "trigger", false);
    lab.run(0.1, 0.016);
    const Spectrum free = waveOf(lab, node);
    CHECK(free.bands[0] < -0.8F);  // freies Fenster: Beginn bei Phase ~4,35 rad, also nahe dem Minimum
}

TEST_CASE("Audio Waveform: Verstärkung begrenzt auf -1..1, Glättung, kein Signal", "[audio][wave]") {
    Lab lab;
    const NodeId node = waveWithScope(lab);
    audio::AudioFrame constant;
    constant.sampleRate = 48000.0;
    constant.recent.assign(audio::kRecentSamples, 0.4F);
    lab.audio->write(constant);
    lab.set(node, "trigger", false);

    lab.run(0.0, 0.016);
    CHECK(waveOf(lab, node).bands[5] == Approx(0.4F));
    lab.set(node, "gain", 10.0);
    lab.run(0.1, 0.016);
    CHECK(waveOf(lab, node).bands[5] == 1.0F);

    lab.set(node, "gain", 1.0);
    lab.set(node, "smooth", 0.5);
    lab.run(0.2, 0.016);
    CHECK(waveOf(lab, node).bands[5] == Approx(0.7F));  // halb zwischen 1,0 (vorher) und 0,4 (neu)

    lab.audioNow += std::chrono::seconds{2};  // Signal weg: die Kurve läuft gegen 0
    lab.set(node, "smooth", 0.0);
    lab.run(0.3, 0.016);
    CHECK(waveOf(lab, node).bands[5] == 0.0F);
    CHECK_FALSE(std::get<bool>(*lab.store.value(node, "present")));
}

TEST_CASE("Oscilloscope: Spektrum und Wellenform sind nicht vertauschbar, der Slot hat Platz für 32 Werte",
          "[audio][wave]") {
    Lab lab;
    NodeId scope{};
    const NodeId wave = waveWithScope(lab, &scope);
    const NodeId spectrum = lab.add("wh.value.audiospectrum");
    const NodeId bars = lab.add("wh.generator.spectrum");
    CHECK_FALSE(lab.graph.connect({spectrum, "spectrum"}, {scope, "wave"}).ok());
    CHECK_FALSE(lab.graph.connect({wave, "wave"}, {bars, "spectrum"}).ok());

    const auto compiled = compiler::compileField(lab.graph, lab.snippets, graph::PortRef{scope, "out"});
    REQUIRE(compiled.ok());
    CHECK(compiled.value().wgsl.find("@spectrum") == std::string::npos);  // Platzhalter vollständig ersetzt
    const compiler::ParameterSlot* slot = slotFor(compiled.value().layout, scope, "wave");
    REQUIRE(slot != nullptr);
    CHECK(slot->origin == compiler::SlotOrigin::SpectrumInput);

    lab.audio->write(sineFrame(0.0));
    lab.run(0.0, 0.016);
    const std::vector<std::byte> bytes = compiler::packParameters(lab.graph, compiled.value().layout, &lab.store);
    const Spectrum expected = waveOf(lab, wave);
    for (const std::size_t i : {std::size_t{0}, std::size_t{8}, std::size_t{24}, std::size_t{31}}) {
        CHECK(test::floatAt(bytes, slot->offsetBytes + static_cast<std::uint32_t>(i) * 4U) == Approx(expected.bands[i]));
    }
}
