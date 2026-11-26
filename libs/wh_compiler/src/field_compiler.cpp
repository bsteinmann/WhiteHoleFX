// field_compiler.cpp: Umsetzung zu field_compiler.hpp. Verschmilzt die Field-Nodes eines Graphen zu einer WGSL-Funktion.
#include "whitehole/compiler/field_compiler.hpp"

#include <cstddef>
#include <map>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "shader_text.hpp"
#include "text_bindings.hpp"
#include "whitehole/compiler/snippet_expander.hpp"
#include "whitehole/core/hash.hpp"

namespace whitehole::compiler {

namespace {

using detail::bindingLine;
using detail::describeNode;
using detail::functionName;
using detail::indent;
using detail::memberRef;

using graph::NodeDescriptor;
using graph::NodeInstance;
using graph::PortDescriptor;
using graph::PortDirection;
using graph::PortType;

std::size_t countFieldOutputs(const NodeDescriptor& descriptor) {
    std::size_t count = 0;
    for (const PortDescriptor& port : descriptor.ports) {
        if (port.direction == PortDirection::Output && port.type == PortType::Field) {
            ++count;
        }
    }
    return count;
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

    Result<CompiledField> compile(const graph::PortRef& target, std::span<const graph::PortRef> extras) {
        if (Result<void> checked = checkTarget(target); !checked.ok()) {
            return checked.error();
        }
        if (Result<void> visited = visit(target.node); !visited.ok()) {
            return visited.error();
        }
        std::vector<NodeId> extraNodes;
        for (const graph::PortRef& extra : extras) {
            if (Result<void> checked = checkTarget(extra); !checked.ok()) {
                return checked.error();
            }
            if (Result<void> visited = visit(extra.node); !visited.ok()) {
                return visited.error();
            }
            extraNodes.push_back(extra.node);
        }
        return assemble(target.node, extraNodes);
    }

private:
    Result<void> checkTarget(const graph::PortRef& target) const {
        const NodeInstance* node = graph_.findNode(target.node);
        if (node == nullptr) {
            return Error{ErrorCode::NotFound, "Ziel-Node existiert nicht"};
        }
        const NodeDescriptor* descriptor = graph_.registry().find(node->typeId);
        const PortDescriptor* port =
            descriptor == nullptr ? nullptr : descriptor->findPort(target.portKey);
        if (port == nullptr) {
            return Error{ErrorCode::NotFound, describeNode(*node) + ": Ziel-Port existiert nicht"};
        }
        if (port->direction != PortDirection::Output || port->type != PortType::Field) {
            return Error{ErrorCode::InvalidArgument,
                         describeNode(*node) + ": Das Ziel muss ein Field-Ausgang sein"};
        }
        return {};
    }

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
            return Error{ErrorCode::NotFound, describeNode(*node) + ": Node-Typ ist nicht registriert"};
        }
        if (descriptor->domain != graph::NodeDomain::Field) {
            return Error{ErrorCode::Unsupported,
                         describeNode(*node) + " liegt nicht in der Field-Domäne. CPU-Nodes liefern "
                                           "Werte über wh_values; Raster-Nodes folgen mit den "
                                           "Raster-Pässen (Schritt 4b, Teil B)"};
        }
        if (countFieldOutputs(*descriptor) != 1) {
            return Error{ErrorCode::Unsupported,
                         describeNode(*node) + ": Field-Nodes brauchen genau einen Field-Ausgang"};
        }
        if (node->bypassed) {
            return visitBypassed(*node, *descriptor);
        }
        const NodeSnippet* snippet = snippets_.find(node->typeId);
        if (snippet == nullptr) {
            return Error{ErrorCode::NotFound, describeNode(*node) + ": kein Shader-Snippet vorhanden"};
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

    /// Überbrückter Node: reicht den ersten Field-Eingang unverändert weiter. Ist dort nichts verbunden (oder
    /// hat der Node keinen Field-Eingang, etwa ein Generator), ist das Ergebnis transparent. Die übrigen
    /// Eingänge und die Parameter spielen keine Rolle: Pflicht-Eingänge werden nicht geprüft, Uniform-Slots
    /// nicht angelegt, und Nodes davor, die sonst nirgends gebraucht werden, entfallen aus dem Shader.
    Result<void> visitBypassed(const NodeInstance& node, const NodeDescriptor& descriptor) {
        std::string body = "return vec4<f32>(0.0, 0.0, 0.0, 0.0);";
        visiting_.insert(node.id);
        for (const PortDescriptor& port : descriptor.ports) {
            if (port.direction != PortDirection::Input || port.type != PortType::Field) {
                continue;
            }
            if (const graph::Connection* connection = graph_.findInputConnection(node.id, port.key); connection != nullptr) {
                const NodeInstance* source = graph_.findNode(connection->from.node);
                const NodeDescriptor* sourceDescriptor =
                    source == nullptr ? nullptr : graph_.registry().find(source->typeId);
                const PortDescriptor* sourcePort =
                    sourceDescriptor == nullptr ? nullptr : sourceDescriptor->findPort(connection->from.portKey);
                if (sourcePort != nullptr && sourcePort->type == PortType::Field) {
                    if (Result<void> visited = visit(connection->from.node); !visited.ok()) {
                        return visited;
                    }
                    body = "return " + functionName(connection->from.node) + "(p);";
                }
            }
            break;  // nur der erste Field-Eingang zählt
        }
        functions_ += "fn " + functionName(node.id) + "(p: vec3<f32>) -> vec4<f32> {\n" + indent(body) + "}\n\n";
        order_.push_back(node.id);
        emitted_.insert(node.id);
        visiting_.erase(node.id);
        return {};
    }

    /// Legt für einen Scalar oder eine Farbe am Field-Eingang einen Uniform-Slot an
    /// und liefert den Ausdruck, der im Shader als konstantes Feld dient.
    InputBinding addConstant(const NodeInstance& node, const std::string& portKey, PortType type) {
        const bool isColor = type == PortType::Color;
        layout_.add(node.id, portKey, isColor ? GpuType::Vec4 : GpuType::Float,
                    SlotOrigin::FieldConstant);
        const std::string member = "wh_params." + memberRef(node.id, portKey);
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

            if (graph::carriesBands(port.type)) {
                // Ein Spektrum oder eine Wellenform: eigener Slot (128 Bytes), den packParameters aus dem Werte-Kanal füllt.
                // Unverbunden bleibt er null (der Shader sieht Stille).
                if (connection != nullptr) {
                    const NodeInstance* source = graph_.findNode(connection->from.node);
                    const NodeDescriptor* sourceDescriptor =
                        source == nullptr ? nullptr : graph_.registry().find(source->typeId);
                    const PortDescriptor* sourcePort =
                        sourceDescriptor == nullptr ? nullptr : sourceDescriptor->findPort(connection->from.portKey);
                    if (sourcePort == nullptr || sourcePort->type != port.type) {
                        return Error{ErrorCode::NotFound, describeNode(node) + ": Die Quelle des Eingangs '" +
                                                              port.key + "' hat nicht den passenden Typ (Spektrum oder Wellenform)"};
                    }
                }
                layout_.add(node.id, port.key, GpuType::Spectrum, SlotOrigin::SpectrumInput);
                continue;
            }
            if (port.type != PortType::Field) {
                // Modulation: Der Eingang gehört zu einem Parameter gleichen Schlüssels,
                // dessen Slot im Uniform-Puffer den verbundenen Wert erhält. Am Shader
                // ändert sich nichts. Ohne solchen Parameter hat der Wert kein Ziel.
                if (connection != nullptr && descriptor.findParameter(port.key) == nullptr) {
                    return Error{ErrorCode::Unsupported,
                                 describeNode(node) + ": Der Eingang '" + port.key +
                                     "' hat keinen gleichnamigen Parameter, den er modulieren "
                                     "könnte (siehe parameter_resolve.hpp)"};
                }
                continue;
            }
            if (connection == nullptr) {
                if (graph::isRequiredInput(port, graph_.strictInputs())) {
                    return Error{ErrorCode::InvalidArgument,
                                 describeNode(node) + ": Pflicht-Eingang '" + port.key +
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
                return Error{ErrorCode::NotFound, describeNode(node) + ": Die Quelle des Eingangs '" +
                                                      port.key + "' existiert nicht mehr"};
            }
            if (sourcePort->type == PortType::Scalar || sourcePort->type == PortType::Color) {
                inputCalls[port.key] = addConstant(node, port.key, sourcePort->type);
                continue;
            }
            if (sourcePort->type != PortType::Field) {
                return Error{ErrorCode::Unsupported,
                             describeNode(node) + ": Bilder als Feld-Eingang folgen mit den "
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

    Result<std::string> expandBody(const NodeInstance& node, const NodeDescriptor& descriptor,
                                   const NodeSnippet& snippet, const InputBindings& inputCalls) {
        SnippetBindings bindings;
        bindings.time = "wh_frame.time";
        bindings.volume = "(wh_frame.volume != 0u)";
        bindings.aspect = "wh_frame.aspect";
        bindings.aspectZ = "wh_frame.aspectZ";
        bindings.columns = "wh_frame.columns";
        bindings.rows = "wh_frame.rows";
        bindings.input = [&](std::string_view port, std::string_view coordinate) -> Result<std::string> {
            const auto it = inputCalls.find(port);
            if (it == inputCalls.end()) {
                return Error{ErrorCode::InvalidArgument,
                             describeNode(node) + ": @in(" + std::string{port} +
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
        bindings.spectrum = [&](std::string_view port, std::string_view index) -> Result<std::string> {
            const graph::PortDescriptor* found = descriptor.findPort(port);
            if (found == nullptr || !graph::carriesBands(found->type) ||
                found->direction != PortDirection::Input) {
                return Error{ErrorCode::InvalidArgument,
                             describeNode(node) + ": @spectrum(" + std::string{port} +
                                 ") verweist auf keinen Spektrum- oder Wellenform-Eingang"};
            }
            // array<vec4<f32>, 8>: Band i steht in Element i/4, Komponente i%4.
            const std::string member = "wh_params." + memberRef(node.id, port);
            const std::string idx = "(" + std::string{index} + ")";
            return member + "[" + idx + " / 4u][" + idx + " % 4u]";
        };
        detail::bindTextPlaceholders(bindings, node, descriptor);
        bindings.parameter = [&](std::string_view key) -> Result<std::string> {
            const graph::ParameterDescriptor* parameter = descriptor.findParameter(key);
            if (parameter == nullptr || !gpuTypeOf(parameter->defaultValue).has_value()) {
                return Error{ErrorCode::InvalidArgument,
                             describeNode(node) + ": @param(" + std::string{key} +
                                 ") ist kein GPU-fähiger Parameter"};
            }
            return "wh_params." + memberRef(node.id, key);
        };
        // @phase(key): eigener Float-Slot je (Node, Schlüssel) - auch wenn der Platzhalter mehrfach vorkommt -, den
        // packParameters aus der Rate des Parameters und dem PhaseTracker füllt (phase_tracker.hpp).
        bindings.phase = [&](std::string_view key, std::string_view curve) -> Result<std::string> {
            const graph::ParameterDescriptor* parameter = descriptor.findParameter(key);
            if (parameter == nullptr || gpuTypeOf(parameter->defaultValue) != GpuType::Float) {
                return Error{ErrorCode::InvalidArgument,
                             describeNode(node) + ": @phase(" + std::string{key} +
                                 ") ist kein Zahlen-Parameter (Float, Ganzzahl oder Schalter)"};
            }
            if (!isPhaseCurve(curve)) {
                return Error{ErrorCode::InvalidArgument,
                             describeNode(node) + ": @phase(" + std::string{key} + ", " + std::string{curve} +
                                 "): unbekannte Kurve (erlaubt: abs, quadratic, reciprocal)"};
            }
            const std::string slotKey = phaseSlotKey(key, curve);
            if (phaseSlots_.emplace(node.id.value(), slotKey).second) {
                layout_.add(node.id, slotKey, GpuType::Float, SlotOrigin::Phase);
            }
            return "wh_params." + memberRef(node.id, slotKey);
        };

        Result<std::string> expanded = expandSnippet(snippet.body, bindings);
        if (!expanded.ok()) {
            Error error = expanded.error();
            error.message = describeNode(node) + ": " + error.message;
            return error;
        }
        return expanded;
    }

    Result<CompiledField> assemble(NodeId target, const std::vector<NodeId>& extras) const {
        CompiledField result;
        result.layout = layout_.build();
        result.nodes = order_;
        for (ParameterSlot& slot : result.layout.slots) {
            slot.member = memberRef(slot.node, slot.key);  // Platzhalter wie in den Funktionsrümpfen
        }

        result.wgsl =
            "// Von WhiteHoleFX erzeugt (Feldcompiler). Nicht von Hand ändern.\n"
            "struct WhFrame {\n    time: f32,\n    delta: f32,\n    frame: u32,\n    volume: u32,\n    "
            "aspect: f32,\n    aspectZ: f32,\n    columns: f32,\n    rows: f32,\n}\n" +
            bindingLine(kFrameBinding, "wh_frame: WhFrame") + "\n" +
            wgslParameterStruct(result.layout) +
            bindingLine(kParametersBinding, "wh_params: WhParams") + "\n" + preludes_ + functions_ +
            "fn wh_field(p: vec3<f32>) -> vec4<f32> {\n    return " + functionName(target) +
            "(p);\n}\n";
        for (std::size_t i = 0; i < extras.size(); ++i) {
            result.wgsl += "fn wh_field_extra" + std::to_string(i) + "(p: vec3<f32>) -> vec4<f32> {\n    return " +
                           functionName(extras[i]) + "(p);\n}\n";
        }

        // Namen nach Ausgabereihenfolge statt Node-Nummer: gleiche Struktur, gleicher Text (shader_text.hpp)
        detail::resolveShaderNames(result.wgsl, result.layout, order_);
        result.structureHash = fnv1a64(result.wgsl);
        return result;
    }

    const graph::Graph& graph_;
    const ISnippetLibrary& snippets_;
    LayoutBuilder layout_;
    std::vector<NodeId> order_;
    std::unordered_set<NodeId> emitted_;
    std::unordered_set<NodeId> visiting_;
    std::set<std::pair<NodeId::rep_type, std::string>> phaseSlots_;
    std::set<std::string> seenPreludes_;
    std::string preludes_;
    std::string functions_;
};

}  // namespace

Result<CompiledField> compileField(const graph::Graph& graph, const ISnippetLibrary& snippets,
                                   const graph::PortRef& target, std::span<const graph::PortRef> extras) {
    FieldCompiler compiler{graph, snippets};
    return compiler.compile(target, extras);
}

}  // namespace whitehole::compiler
