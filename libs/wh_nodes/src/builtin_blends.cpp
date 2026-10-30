// Mischer: verrechnen zwei Felder an derselben Koordinate.
#include "builtin_internal.hpp"

namespace whitehole::nodes::detail {

namespace {

constexpr const char* kMixBody =
    R"wgsl(return mix(@in(a, p), @in(b, p), clamp(@param(amount), 0.0, 1.0));)wgsl";

constexpr const char* kAddBody =
    R"wgsl(return clamp(@in(a, p) + @in(b, p), vec4<f32>(0.0, 0.0, 0.0, 0.0), vec4<f32>(1.0, 1.0, 1.0, 1.0));)wgsl";

}  // namespace

std::vector<compiler::NodeDefinition> blendNodes() {
    std::vector<compiler::NodeDefinition> nodes;

    nodes.push_back(fieldNode("wh.blend.mix", "Mix", "Blend",
                              {fieldInput("a", "A"), fieldInput("b", "B"), fieldOutput()},
                              {number("amount", "Amount", 0.5, 0.0, 1.0)}, kMixBody));

    nodes.push_back(fieldNode("wh.blend.add", "Add", "Blend",
                              {fieldInput("a", "A"), fieldInput("b", "B"), fieldOutput()}, {},
                              kAddBody));

    return nodes;
}

}  // namespace whitehole::nodes::detail
