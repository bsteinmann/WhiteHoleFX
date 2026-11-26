// shader_text.cpp: Umsetzung zu shader_text.hpp. Warum die Namen erst am Ende vergeben werden, steht dort.
#include "shader_text.hpp"

#include <cstdint>
#include <unordered_map>

#include "whitehole/compiler/field_compiler.hpp"

namespace whitehole::compiler::detail {

namespace {

// Steuerzeichen kommen in WGSL sonst nicht vor; zwischen ihnen steht die Node-Nummer.
constexpr char kOpen = '\x01';
constexpr char kClose = '\x02';

std::string placeholder(NodeId id) { return kOpen + std::to_string(id.value()) + kClose; }

}  // namespace

std::string functionName(NodeId id) { return "wh_f" + placeholder(id); }

std::string memberRef(NodeId id, std::string_view key) { return "n" + placeholder(id) + "_" + std::string{key}; }

void resolveShaderNames(std::string& wgsl, ParameterLayout& layout, const std::vector<NodeId>& order) {
    std::unordered_map<NodeId::rep_type, std::size_t> position;
    for (std::size_t i = 0; i < order.size(); ++i) {
        position.emplace(order[i].value(), i + 1);
    }
    const auto numberOf = [&position](NodeId::rep_type id) {
        const auto found = position.find(id);
        return std::to_string(found != position.end() ? found->second : id);
    };

    std::string out;
    out.reserve(wgsl.size());
    std::size_t start = 0;
    while (start < wgsl.size()) {
        const std::size_t open = wgsl.find(kOpen, start);
        const std::size_t close = open == std::string::npos ? std::string::npos : wgsl.find(kClose, open);
        if (close == std::string::npos) {
            out.append(wgsl, start, std::string::npos);
            break;
        }
        out.append(wgsl, start, open - start);
        out += numberOf(static_cast<NodeId::rep_type>(std::stoull(wgsl.substr(open + 1, close - open - 1))));
        start = close + 1;
    }
    wgsl = std::move(out);

    for (ParameterSlot& slot : layout.slots) {
        slot.member = "n" + numberOf(slot.node.value()) + "_" + slot.key;
    }
}

std::string describeNode(const graph::NodeInstance& node) {
    return "Node #" + std::to_string(node.id.value()) + " (" + node.typeId + ")";
}

std::string indent(const std::string& text) {
    std::string result;
    const std::string_view view{text};
    std::size_t start = 0;
    while (start < view.size()) {
        std::size_t end = view.find('\n', start);
        if (end == std::string_view::npos) {
            end = view.size();
        }
        const std::string_view line = view.substr(start, end - start);
        if (!line.empty()) {
            result += "    ";
            result += line;
        }
        result += '\n';
        start = end + 1;
    }
    return result;
}

std::string bindingLine(std::uint32_t binding, const std::string& declaration) {
    return "@group(" + std::to_string(kFieldBindGroup) + ") @binding(" + std::to_string(binding) +
           ") var<uniform> " + declaration + ";\n";
}

}  // namespace whitehole::compiler::detail
