// parameter_layout.cpp: Umsetzung zu parameter_layout.hpp. Lage der Node-Parameter im Uniform-Puffer des Feldshaders.
#include "whitehole/compiler/parameter_layout.hpp"

#include <array>
#include <cstring>
#include <optional>
#include <utility>
#include <variant>

#include "whitehole/graph/parameter_resolve.hpp"

namespace whitehole::compiler {

namespace {

struct SizeAlign {
    std::uint32_t size;
    std::uint32_t align;
};

SizeAlign sizeAlignOf(GpuType type) {
    switch (type) {
        case GpuType::Float:
            return {4, 4};
        case GpuType::Vec2:
            return {8, 8};
        case GpuType::Vec3:
            return {12, 16};
        case GpuType::Vec4:
            return {16, 16};
        case GpuType::Spectrum:
            return {static_cast<std::uint32_t>(kSpectrumBands * sizeof(float)), 16};
    }
    return {4, 4};
}

std::uint32_t roundUp(std::uint32_t value, std::uint32_t alignment) {
    return (value + alignment - 1U) / alignment * alignment;
}

/// Wandelt einen Parameterwert in bis zu vier Floats. Gibt die Anzahl zurück
/// (0 bei Typen ohne GPU-Entsprechung).
std::size_t toFloats(const graph::ParameterValue& value, std::array<float, 4>& out) {
    if (const auto* flag = std::get_if<bool>(&value)) {
        out[0] = *flag ? 1.0F : 0.0F;
        return 1;
    }
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        out[0] = static_cast<float>(*integer);
        return 1;
    }
    if (const auto* number = std::get_if<double>(&value)) {
        out[0] = static_cast<float>(*number);
        return 1;
    }
    if (const auto* v2 = std::get_if<Vec2>(&value)) {
        out = {v2->x, v2->y, 0.0F, 0.0F};
        return 2;
    }
    if (const auto* v3 = std::get_if<Vec3>(&value)) {
        out = {v3->x, v3->y, v3->z, 0.0F};
        return 3;
    }
    if (const auto* color = std::get_if<ColorRgba>(&value)) {
        out = {color->r, color->g, color->b, color->a};
        return 4;
    }
    return 0;
}

std::size_t floatCountOf(GpuType type) {
    switch (type) {
        case GpuType::Float:
            return 1;
        case GpuType::Vec2:
            return 2;
        case GpuType::Vec3:
            return 3;
        case GpuType::Vec4:
            return 4;
        case GpuType::Spectrum:
            return kSpectrumBands;
    }
    return 1;
}

}  // namespace

std::optional<GpuType> gpuTypeOf(const graph::ParameterValue& value) noexcept {
    switch (value.index()) {
        case 0:  // bool
        case 1:  // int64
        case 2:  // double
            return GpuType::Float;
        case 3:
            return GpuType::Vec2;
        case 4:
            return GpuType::Vec3;
        case 5:
            return GpuType::Vec4;
        default:
            return std::nullopt;  // std::string
    }
}

std::string_view wgslTypeName(GpuType type) noexcept {
    switch (type) {
        case GpuType::Float:
            return "f32";
        case GpuType::Vec2:
            return "vec2<f32>";
        case GpuType::Vec3:
            return "vec3<f32>";
        case GpuType::Vec4:
            return "vec4<f32>";
        case GpuType::Spectrum:
            return "array<vec4<f32>, 8>";
    }
    return "f32";
}

std::string memberName(NodeId node, std::string_view key) {
    return "n" + std::to_string(node.value()) + "_" + std::string{key};
}

void LayoutBuilder::add(NodeId node, std::string key, GpuType type, SlotOrigin origin) {
    const SizeAlign layout = sizeAlignOf(type);
    ParameterSlot slot;
    slot.node = node;
    slot.type = type;
    slot.origin = origin;
    slot.offsetBytes = roundUp(end_, layout.align);
    slot.member = memberName(node, key);
    slot.key = std::move(key);
    end_ = slot.offsetBytes + layout.size;
    slots_.push_back(std::move(slot));
}

ParameterLayout LayoutBuilder::build() const {
    ParameterLayout layout;
    layout.slots = slots_;
    layout.sizeBytes = roundUp(end_, 16U) + 16U;  // Abschlusselement vec4
    return layout;
}

std::string wgslParameterStruct(const ParameterLayout& layout) {
    std::string text = "struct WhParams {\n";
    for (const ParameterSlot& slot : layout.slots) {
        text += "    " + slot.member + ": " + std::string{wgslTypeName(slot.type)} + ",\n";
    }
    text += "    _pad: vec4<f32>,\n}\n";
    return text;
}

namespace {

/// Der aktuelle Wert eines Slots oder nichts, wenn es keinen gibt.
std::optional<graph::ParameterValue> valueOfSlot(const graph::Graph& graph,
                                                 const graph::NodeInstance& node,
                                                 const ParameterSlot& slot,
                                                 const graph::IValueLookup* values) {
    if (slot.origin == SlotOrigin::FieldConstant) {
        const graph::Connection* connection = graph.findInputConnection(node.id, slot.key);
        if (connection == nullptr || values == nullptr) {
            return std::nullopt;
        }
        return values->value(connection->from.node, connection->from.portKey);
    }

    const graph::NodeDescriptor* descriptor = graph.registry().find(node.typeId);
    const graph::ParameterDescriptor* parameter =
        descriptor == nullptr ? nullptr : descriptor->findParameter(slot.key);
    if (parameter != nullptr) {
        return graph::resolveParameter(graph, node, *parameter, values);
    }
    const auto stored = node.parameters.find(slot.key);
    if (stored == node.parameters.end()) {
        return std::nullopt;
    }
    return stored->second;
}

}  // namespace

std::vector<std::byte> packParameters(const graph::Graph& graph, const ParameterLayout& layout,
                                      const graph::IValueLookup* values) {
    std::vector<std::byte> bytes(layout.sizeBytes, std::byte{0});

    for (const ParameterSlot& slot : layout.slots) {
        const graph::NodeInstance* node = graph.findNode(slot.node);
        if (node == nullptr) {
            continue;
        }
        if (slot.origin == SlotOrigin::SpectrumInput) {
            const graph::Connection* connection = graph.findInputConnection(node->id, slot.key);
            if (connection != nullptr && values != nullptr) {
                if (const auto spectrum = values->spectrum(connection->from.node, connection->from.portKey)) {
                    std::memcpy(bytes.data() + slot.offsetBytes, spectrum->bands.data(),
                                spectrum->bands.size() * sizeof(float));
                }
            }
            continue;
        }
        const std::optional<graph::ParameterValue> value = valueOfSlot(graph, *node, slot, values);
        if (!value.has_value()) {
            continue;
        }

        std::array<float, 4> floats{};
        const std::size_t count = toFloats(*value, floats);
        if (count == 0 || count != floatCountOf(slot.type)) {
            continue;  // Typ passt nicht (mehr) zum Slot
        }
        std::memcpy(bytes.data() + slot.offsetBytes, floats.data(), count * sizeof(float));
    }
    return bytes;
}

}  // namespace whitehole::compiler
