#include "whitehole/compiler/field_compiler.hpp"

#include <cstddef>
#include <map>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "whitehole/compiler/snippet_expander.hpp"
#include "whitehole/core/hash.hpp"

namespace whitehole::compiler {

namespace {

using graph::NodeDescriptor;
using graph::NodeInstance;
using graph::PortDescriptor;
using graph::PortDirection;
using graph::PortType;

std::string describe(const NodeInstance& node) {
    return "Node #" + std::to_string(node.id.value()) + " (" + node.typeId + ")";
}

std::string functionName(NodeId id) {
    return "wh_f" + std::to_string(id.value());
}

std::size_t countFieldOutputs(const NodeDescriptor& descriptor) {
    std::size_t count = 0;
    for (const PortDescriptor& port : descriptor.ports) {
        if (port.direction == PortDirection::Output && port.type == PortType::Field) {
            ++count;
        }
    }
    return count;
}

/// Rückt jede nichtleere Zeile um vier Leerzeichen ein und schließt mit Zeilenumbruch ab.
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

/// Wie ein Field-Eingang im Snippet aufgelöst wird.
struct InputBinding {
    enum class Kind {
        Zero,      // nicht verbundener optionaler Eingang: transparentes Schwarz
        Call,      // Funktion eines anderen Field-Nodes, wird an der Koordinate aufgerufen
        Constant,  // fertiger Ausdruck (Uniform-Wert), die Koordinate spielt keine Rolle
    };
    Kind kind{Kind::Zero};
    std::string text;
};

using InputBindings = std::map<std::string, InputBinding, std::less<>>;

class FieldCompiler {
public:
    FieldCompiler(const graph::Graph& graph, const ISnippetLibrary& snippets)
        : graph_{graph}, snippets_{snippets} {}

    Result<CompiledField> compile(const graph::PortRef& target) {
        const NodeInstance* node = graph_.findNode(target.node);
        if (node == nullptr) {
            return Error{ErrorCode::NotFound, "Ziel-Node existiert nicht"};
        }
        const NodeDescriptor* descriptor = graph_.registry().find(node->typeId);
        const PortDescriptor* port =
            descriptor == nullptr ? nullptr : descriptor->findPort(target.portKey);
        if (port == nullptr) {
            return Error{ErrorCode::NotFound, describe(*node) + ": Ziel-Port existiert nicht"};
        }
        if (port->direction != PortDirection::Output || port->type != PortType::Field) {
            return Error{ErrorCode::InvalidArgument,
                         describe(*node) + ": Das Ziel muss ein Field-Ausgang sein"};
        }

        if (Result<void> visited = visit(target.node); !visited.ok()) {
            return visited.error();
        }
        return assemble(target.node);
    }

private:
    Result<void> visit(NodeId id) {
        if (emitted_.contains(id)) {
            return {};
        }
        if (visiting_.contains(id)) {
            return Error{ErrorCode::CycleDetected, "Zyklus bei Node #" + std::to_string(id.value())};
        }

        const NodeInstance* node = graph_.findNode(id);
        if (node == nullptr) {
            return Error{ErrorCode::NotFound, "Node #" + std::to_string(id.value()) + " existiert nicht"};
        }
        const NodeDescriptor* descriptor = graph_.registry().find(node->typeId);
        if (descriptor == nullptr) {
            return Error{ErrorCode::NotFound, describe(*node) + ": Node-Typ ist nicht registriert"};
        }
        if (descriptor->domain != graph::NodeDomain::Field) {
            return Error{ErrorCode::Unsupported,
                         describe(*node) + " liegt nicht in der Field-Domäne. CPU-Nodes liefern "
                                           "Werte über wh_values; Raster-Nodes folgen mit den "
                                           "Raster-Pässen (Schritt 4b, Teil B)"};
        }
        if (countFieldOutputs(*descriptor) != 1) {
            return Error{ErrorCode::Unsupported,
                         describe(*node) + ": Field-Nodes brauchen genau einen Field-Ausgang"};
        }
        const NodeSnippet* snippet = snippets_.find(node->typeId);
        if (snippet == nullptr) {
            return Error{ErrorCode::NotFound, describe(*node) + ": kein Shader-Snippet vorhanden"};
        }

        visiting_.insert(id);
        InputBindings inputCalls;
        if (Result<void> resolved = resolveInputs(*node, *descriptor, inputCalls); !resolved.ok()) {
            return resolved;
        }

        for (const graph::ParameterDescriptor& parameter : descriptor->parameters) {
            if (const auto type = gpuTypeOf(parameter.defaultValue); type.has_value()) {
                layout_.add(id, parameter.key, *type);
            }
        }

        Result<std::string> body = expandBody(*node, *descriptor, *snippet, inputCalls);
        if (!body.ok()) {
            return body.error();
        }

        if (!snippet->prelude.empty() && seenPreludes_.insert(snippet->prelude).second) {
            preludes_ += snippet->prelude;
            preludes_ += snippet->prelude.back() == '\n' ? "\n" : "\n\n";
        }
        functions_ += "fn " + functionName(id) + "(p: vec3<f32>) -> vec4<f32> {\n" +
                      indent(body.value()) + "}\n\n";

        order_.push_back(id);
        emitted_.insert(id);
        visiting_.erase(id);
        return {};
    }

    /// Legt für einen Scalar oder eine Farbe am Field-Eingang einen Uniform-Slot an
    /// und liefert den Ausdruck, der im Shader als konstantes Feld dient.
    InputBinding addConstant(const NodeInstance& node, const std::string& portKey, PortType type) {
        const bool isColor = type == PortType::Color;
        layout_.add(node.id, portKey, isColor ? GpuType::Vec4 : GpuType::Float,
                    SlotOrigin::FieldConstant);
        const std::string member = "wh_params." + memberName(node.id, portKey);
        if (isColor) {
            return InputBinding{InputBinding::Kind::Constant, member};
        }
        return InputBinding{InputBinding::Kind::Constant,
                            "vec4<f32>(" + member + ", " + member + ", " + member + ", 1.0)"};
    }

    Result<void> resolveInputs(const NodeInstance& node, const NodeDescriptor& descriptor,
                               InputBindings& inputCalls) {
        for (const PortDescriptor& port : descriptor.ports) {
            if (port.direction != PortDirection::Input) {
                continue;
            }
            const graph::Connection* connection = graph_.findInputConnection(node.id, port.key);

            if (port.type != PortType::Field) {
                // Modulation: Der Eingang gehört zu einem Parameter gleichen Schlüssels,
                // dessen Slot im Uniform-Puffer den verbundenen Wert erhält. Am Shader
                // ändert sich nichts. Ohne solchen Parameter hat der Wert kein Ziel.
                if (connection != nullptr && descriptor.findParameter(port.key) == nullptr) {
                    return Error{ErrorCode::Unsupported,
                                 describe(node) + ": Der Eingang '" + port.key +
                                     "' hat keinen gleichnamigen Parameter, den er modulieren "
                                     "könnte (siehe parameter_resolve.hpp)"};
                }
                continue;
            }
            if (connection == nullptr) {
                if (port.required) {
                    return Error{ErrorCode::InvalidArgument,
                                 describe(node) + ": Pflicht-Eingang '" + port.key +
                                     "' ist nicht verbunden"};
                }
                inputCalls[port.key] = InputBinding{};
                continue;
            }

            const NodeInstance* source = graph_.findNode(connection->from.node);
            const NodeDescriptor* sourceDescriptor =
                source == nullptr ? nullptr : graph_.registry().find(source->typeId);
            const PortDescriptor* sourcePort =
                sourceDescriptor == nullptr ? nullptr
                                            : sourceDescriptor->findPort(connection->from.portKey);
            if (sourcePort == nullptr) {
                return Error{ErrorCode::NotFound, describe(node) + ": Die Quelle des Eingangs '" +
                                                      port.key + "' existiert nicht mehr"};
            }
            if (sourcePort->type == PortType::Scalar || sourcePort->type == PortType::Color) {
                inputCalls[port.key] = addConstant(node, port.key, sourcePort->type);
                continue;
            }
            if (sourcePort->type != PortType::Field) {
                return Error{ErrorCode::Unsupported,
                             describe(node) + ": Bilder als Feld-Eingang folgen mit den "
                                              "Raster-Pässen (Schritt 4b, Teil B)"};
            }
            if (Result<void> visited = visit(connection->from.node); !visited.ok()) {
                return visited;
            }
            inputCalls[port.key] =
                InputBinding{InputBinding::Kind::Call, functionName(connection->from.node)};
        }
        return {};
    }

    static Result<std::string> expandBody(const NodeInstance& node, const NodeDescriptor& descriptor,
                                          const NodeSnippet& snippet,
                                          const InputBindings& inputCalls) {
        SnippetBindings bindings;
        bindings.time = "wh_frame.time";
        bindings.input = [&](std::string_view port, std::string_view coordinate) -> Result<std::string> {
            const auto it = inputCalls.find(port);
            if (it == inputCalls.end()) {
                return Error{ErrorCode::InvalidArgument,
                             describe(node) + ": @in(" + std::string{port} +
                                 ") verweist auf keinen Field-Eingang"};
            }
            switch (it->second.kind) {
                case InputBinding::Kind::Call:
                    return it->second.text + "(" + std::string{coordinate} + ")";
                case InputBinding::Kind::Constant:
                    return it->second.text;
                case InputBinding::Kind::Zero:
                    break;
            }
            return std::string{"vec4<f32>(0.0, 0.0, 0.0, 0.0)"};
        };
        bindings.parameter = [&](std::string_view key) -> Result<std::string> {
            const graph::ParameterDescriptor* parameter = descriptor.findParameter(key);
            if (parameter == nullptr || !gpuTypeOf(parameter->defaultValue).has_value()) {
                return Error{ErrorCode::InvalidArgument,
                             describe(node) + ": @param(" + std::string{key} +
                                 ") ist kein GPU-fähiger Parameter"};
            }
            return "wh_params." + memberName(node.id, key);
        };

        Result<std::string> expanded = expandSnippet(snippet.body, bindings);
        if (!expanded.ok()) {
            Error error = expanded.error();
            error.message = describe(node) + ": " + error.message;
            return error;
        }
        return expanded;
    }

    Result<CompiledField> assemble(NodeId target) const {
        CompiledField result;
        result.layout = layout_.build();
        result.nodes = order_;

        result.wgsl =
            "// Von WhiteHoleFX erzeugt (Feldcompiler). Nicht von Hand ändern.\n"
            "struct WhFrame {\n    time: f32,\n    delta: f32,\n    frame: u32,\n    _pad: u32,\n}\n" +
            bindingLine(kFrameBinding, "wh_frame: WhFrame") + "\n" +
            wgslParameterStruct(result.layout) +
            bindingLine(kParametersBinding, "wh_params: WhParams") + "\n" + preludes_ + functions_ +
            "fn wh_field(p: vec3<f32>) -> vec4<f32> {\n    return " + functionName(target) +
            "(p);\n}\n";

        result.structureHash = fnv1a64(result.wgsl);
        return result;
    }

    const graph::Graph& graph_;
    const ISnippetLibrary& snippets_;
    LayoutBuilder layout_;
    std::vector<NodeId> order_;
    std::unordered_set<NodeId> emitted_;
    std::unordered_set<NodeId> visiting_;
    std::set<std::string> seenPreludes_;
    std::string preludes_;
    std::string functions_;
};

}  // namespace

Result<CompiledField> compileField(const graph::Graph& graph, const ISnippetLibrary& snippets,
                                   const graph::PortRef& target) {
    FieldCompiler compiler{graph, snippets};
    return compiler.compile(target);
}

}  // namespace whitehole::compiler
