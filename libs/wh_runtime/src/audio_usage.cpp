// audio_usage.cpp: Umsetzung zu audio_usage.hpp. Zählt die Nodes, die aus dem Audio-Eingang lesen.
#include "whitehole/runtime/audio_usage.hpp"

namespace whitehole::runtime {

AudioUsage findAudioUsage(const graph::Graph& graph) {
    AudioUsage usage;
    for (const graph::NodeInstance& node : graph.nodes()) {
        if (node.typeId == "wh.value.audiolevel" || node.typeId == "wh.value.audioband" ||
            node.typeId == "wh.value.audiospectrum" || node.typeId == "wh.value.beat") {
            ++usage.nodes;
        }
    }
    return usage;
}

}  // namespace whitehole::runtime
