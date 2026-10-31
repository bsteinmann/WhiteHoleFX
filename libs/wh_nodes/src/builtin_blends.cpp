// Mischer: verrechnen zwei Felder an derselben Koordinate.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kMixBody =
    R"wgsl(return wh_mix_color(@in(a, p), @in(b, p), clamp(@param(amount), 0.0, 1.0));)wgsl";

constexpr const char* kAddBody =
    R"wgsl(return wh_add_color(@in(a, p), @in(b, p));)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> blendNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode("wh.blend.mix", "Mix", "Blend",
                              {fieldInput("a", "A"), fieldInput("b", "B"), fieldOutput()},
                              {number("amount", "Amount", 0.5, 0.0, 1.0)}, kMixBody, kColorPrelude));

    nodes.push_back(fieldNode("wh.blend.add", "Add", "Blend",
                              {fieldInput("a", "A"), fieldInput("b", "B"), fieldOutput()}, {},
                              kAddBody, kColorPrelude));

    return nodes;
}

}  // namespace whitehole::nodes::detail
