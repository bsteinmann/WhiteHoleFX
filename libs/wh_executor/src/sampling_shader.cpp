// sampling_shader.cpp: Umsetzung zu sampling_shader.hpp. Der Compute-Einstieg, der ein Feld an LED-Positionen abtastet.
#include "whitehole/executor/sampling_shader.hpp"

#include <algorithm>

namespace whitehole::executor {

std::size_t countFieldExtras(std::string_view fieldLibrary) {
    std::size_t count = 0;
    while (fieldLibrary.find("fn wh_field_extra" + std::to_string(count) + "(") != std::string_view::npos) {
        ++count;
    }
    return count;
}

std::string samplingShader(std::string_view fieldLibrary, std::size_t extraCount) {
    std::string text{fieldLibrary};
    text += R"wgsl(
struct WhLeds {
    count: u32,
    extra: u32,
    base: u32,
    tag: u32,
}

@group(1) @binding(0) var<storage, read> wh_positions: array<vec4<f32>>;
@group(1) @binding(1) var<storage, read_write> wh_colors: array<vec4<f32>>;
@group(1) @binding(2) var<uniform> wh_leds: WhLeds;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) id: vec3<u32>, @builtin(num_workgroups) groups: vec3<u32>) {
    let i = id.y * groups.x * 64u + id.x;
    if (i >= wh_leds.count) {
        return;
    }
    wh_colors[i] = wh_field(wh_positions[i].xyz);
)wgsl";
    if (extraCount > 0) {
        text += "    if (wh_leds.extra < " + std::to_string(extraCount) + "u) {\n"
                "        if (i == 0u) {\n"
                "            wh_colors[wh_leds.base] = vec4<f32>(f32(wh_leds.tag), 0.0, 0.0, 0.0);\n"
                "        }\n"
                "        var c = vec4<f32>(0.0);\n";
        for (std::size_t e = 0; e < extraCount; ++e) {
            text += std::string{e == 0 ? "        if" : " else if"} + " (wh_leds.extra == " + std::to_string(e) +
                    "u) {\n            c = wh_field_extra" + std::to_string(e) + "(wh_positions[i].xyz);\n        }";
        }
        text += "\n        wh_colors[wh_leds.base + 1u + i] = c;\n    }\n";
    }
    text += "}\n";
    return text;
}

DispatchSize samplingDispatch(std::uint64_t ledCount) noexcept {
    const std::uint64_t groups = (ledCount + kSamplingWorkgroupSize - 1U) / kSamplingWorkgroupSize;
    if (groups == 0U) {
        return DispatchSize{1, 1, 1};
    }
    const std::uint64_t x = std::min<std::uint64_t>(groups, kMaxGroupsPerDimension);
    const std::uint64_t y = (groups + x - 1U) / x;
    return DispatchSize{static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 1U};
}

}  // namespace whitehole::executor
