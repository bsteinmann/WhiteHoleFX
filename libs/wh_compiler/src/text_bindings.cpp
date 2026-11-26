// text_bindings.cpp: Umsetzung zu text_bindings.hpp. Warum die Textplatzhalter hier liegen, steht dort.
#include "text_bindings.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "shader_text.hpp"
#include "whitehole/core/utf8.hpp"

namespace whitehole::compiler::detail {

namespace {

/// So viele Zeichen eines Textparameters kommen in den Shader (der Rest wird abgeschnitten).
constexpr std::size_t kMaxTextCodePoints = 256;

Result<std::vector<std::uint32_t>> textOf(const graph::NodeInstance& node, const graph::NodeDescriptor& descriptor,
                                          std::string_view key) {
    const graph::ParameterDescriptor* parameter = descriptor.findParameter(key);
    if (parameter == nullptr || !std::holds_alternative<std::string>(parameter->defaultValue)) {
        return Error{ErrorCode::InvalidArgument,
                     describeNode(node) + ": @text(" + std::string{key} + ") ist kein Textparameter"};
    }
    const auto stored = node.parameters.find(std::string{key});
    const std::string& value = stored != node.parameters.end() && std::holds_alternative<std::string>(stored->second)
                                   ? std::get<std::string>(stored->second)
                                   : std::get<std::string>(parameter->defaultValue);
    return Result<std::vector<std::uint32_t>>{decodeUtf8(value, kMaxTextCodePoints)};
}

}  // namespace

void bindTextPlaceholders(SnippetBindings& bindings, const graph::NodeInstance& node,
                          const graph::NodeDescriptor& descriptor) {
    bindings.text = [&node, &descriptor](std::string_view key) -> Result<std::string> {
        Result<std::vector<std::uint32_t>> points = textOf(node, descriptor, key);
        if (!points.ok()) {
            return points.error();
        }
        std::vector<std::uint32_t> list = std::move(points.value());
        if (list.empty()) {
            list.push_back(0);  // WGSL kennt keine Felder der Länge 0
        }
        std::string text = "array<u32, " + std::to_string(list.size()) + ">(";
        for (std::size_t i = 0; i < list.size(); ++i) {
            text += (i == 0 ? "" : ", ") + std::to_string(list[i]) + "u";
        }
        return text + ")";
    };
    bindings.textLength = [&node, &descriptor](std::string_view key) -> Result<std::string> {
        Result<std::vector<std::uint32_t>> points = textOf(node, descriptor, key);
        if (!points.ok()) {
            return points.error();
        }
        return std::to_string(points.value().size()) + "u";
    };
}

}  // namespace whitehole::compiler::detail
