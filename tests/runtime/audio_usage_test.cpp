// findAudioUsage / Player::audioUsage: welche Nodes eines Graphen aus dem Audio-Eingang lesen.
#include <catch2/catch_test_macros.hpp>

#include "whitehole/graph/graph_model.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/audio_usage.hpp"

using namespace whitehole;

TEST_CASE("findAudioUsage: zählt nur Nodes, die Audio lesen", "[runtime][audio]") {
    auto library = nodes::BuiltinLibrary::create();
    REQUIRE(library.ok());
    graph::Graph graph{library.value()->registry};
    CHECK_FALSE(runtime::findAudioUsage(graph).any());

    (void)graph.addNode("wh.value.lfo", Vec2{});
    (void)graph.addNode("wh.value.tap", Vec2{});             // Tap Tempo braucht kein Audio
    (void)graph.addNode("wh.generator.spectrum", Vec2{});    // bekommt das Spektrum über eine Verbindung
    CHECK_FALSE(runtime::findAudioUsage(graph).any());

    for (const char* type : {"wh.value.audiolevel", "wh.value.audioband", "wh.value.audiospectrum", "wh.value.beat",
                              "wh.value.audiowave"}) {
        (void)graph.addNode(type, Vec2{});
    }
    const runtime::AudioUsage usage = runtime::findAudioUsage(graph);
    CHECK(usage.any());
    CHECK(usage.nodes == 5);
}
