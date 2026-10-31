// sampling_shader.cpp: Umsetzung zu sampling_shader.hpp. Der Compute-Einstieg, der ein Feld an LED-Positionen abtastet.
#include "whitehole/executor/sampling_shader.hpp"

#include <algorithm>

namespace whitehole::executor {

std::string samplingShader(std::string_view fieldLibrary) {
    std::string text{fieldLibrary};
    text += R"wgsl(
struct WhLeds {
    count: u32,
    pad0: u32,
    pad1: u32,
    pad2: u32,
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
}
)wgsl";
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
