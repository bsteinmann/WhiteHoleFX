// project_writer.cpp: schreibt ein Projekt als Text, den parseProject wieder lesen kann.
//
// Die Ausgabe ist deterministisch: gleiche Eingabe, gleicher Text. Nodes heißen n1, n2, ...
// in der Reihenfolge des Graphen; Parameter stehen in der Reihenfolge des Node-Typs und
// es werden alle geschrieben, auch die Standardwerte. So ändert sich die Bedeutung einer
// Datei nicht, wenn ein späteres Release einen Standardwert ändert.
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "project_names.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project {

namespace {

using detail::formatDouble;
using detail::formatFloat;
using detail::nameOf;
using detail::quoteWord;

std::string vec2(const Vec2& v) { return formatFloat(v.x) + " " + formatFloat(v.y); }
std::string vec3(const Vec3& v) { return formatFloat(v.x) + " " + formatFloat(v.y) + " " + formatFloat(v.z); }

void writeShape(std::ostream& out, const mapping::FixtureShape& shape) {
    if (const auto* m = std::get_if<mapping::MatrixShape>(&shape)) {
        out << "  matrix " << m->columns << " " << m->rows << " pitch " << vec2(m->pitch) << " start "
            << nameOf(detail::kCorners, m->wiring.start) << " first "
            << nameOf(detail::kFirstAxes, m->wiring.first);
        if (m->wiring.serpentine) {
            out << " serpentine";
        }
        out << "\n";
    } else if (const auto* s = std::get_if<mapping::StripShape>(&shape)) {
        out << "  strip " << s->count << " from " << vec3(s->start) << " to " << vec3(s->end) << "\n";
    } else if (const auto* g = std::get_if<mapping::GridShape>(&shape)) {
        out << "  grid " << g->nx << " " << g->ny << " " << g->nz << " pitch " << vec3(g->pitch) << " order "
            << nameOf(detail::kAxisOrders, g->wiring.order);
        if (g->wiring.serpentine) {
            out << " serpentine";
        }
        out << (g->wiring.flipX ? " flipx" : "") << (g->wiring.flipY ? " flipy" : "") << (g->wiring.flipZ ? " flipz" : "");
        out << "\n";
    } else if (const auto* p = std::get_if<mapping::PointsShape>(&shape)) {
        for (const Vec3& point : p->points) {
            out << "  point " << vec3(point) << "\n";
        }
    }
}

void writePatch(std::ostream& out, const output::FixturePatch& p) {
    out << "  patch " << nameOf(detail::kProtocols, p.protocol) << " destination " << p.destination;
    if (p.protocol == output::Protocol::Ddp) {
        out << " offset " << p.ddpOffset;
    } else {
        out << " universe " << p.universe << " channel " << p.channel;
    }
    out << " order " << nameOf(detail::kColorOrders, p.color.order) << " gamma " << formatFloat(p.color.gamma)
        << " brightness " << formatFloat(p.color.brightness) << "\n";
}

std::string valueText(const graph::ParameterValue& value) {
    return std::visit(
        [](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, bool>) {
                return v ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return std::to_string(v);
            } else if constexpr (std::is_same_v<T, double>) {
                return formatDouble(v);
            } else if constexpr (std::is_same_v<T, Vec2>) {
                return vec2(v);
            } else if constexpr (std::is_same_v<T, Vec3>) {
                return vec3(v);
            } else if constexpr (std::is_same_v<T, ColorRgba>) {
                return formatFloat(v.r) + " " + formatFloat(v.g) + " " + formatFloat(v.b) + " " + formatFloat(v.a);
            } else {
                return quoteWord(v);
            }
        },
        value);
}

/// Nodes, Kabel und Ziel eines Graphen (Hauptgraph oder Look). Nodes heißen n1, n2, ... je Graph.
Result<void> writeGraph(std::ostream& out, const graph::Graph& graph, const std::optional<graph::PortRef>& target,
                        const char* targetName) {
    std::unordered_map<std::uint64_t, std::string> names;
    for (const graph::NodeInstance& node : graph.nodes()) {
        const std::string name = "n" + std::to_string(names.size() + 1);
        names.emplace(node.id.value(), name);
        out << "node " << name << " " << node.typeId << "\n";
        out << "  at " << vec2(node.position) << "\n";
        const graph::NodeDescriptor* descriptor = graph.registry().find(node.typeId);
        if (descriptor != nullptr) {
            for (const graph::ParameterDescriptor& parameter : descriptor->parameters) {
                const auto it = node.parameters.find(parameter.key);
                const graph::ParameterValue& value =
                    it != node.parameters.end() ? it->second : parameter.defaultValue;
                out << "  set " << parameter.key << " " << valueText(value) << "\n";
            }
        }
        out << "end\n\n";
    }

    for (const graph::Connection& c : graph.connections()) {
        out << "connect " << names.at(c.from.node.value()) << "." << c.from.portKey << " -> "
            << names.at(c.to.node.value()) << "." << c.to.portKey << "\n";
    }
    if (target) {
        const auto it = names.find(target->node.value());
        if (it == names.end()) {
            return Error{ErrorCode::InvalidArgument, std::string{targetName} + " verweist auf einen unbekannten Node"};
        }
        out << "output " << it->second << "." << target->portKey << "\n";
    }
    return {};
}

}  // namespace

Result<std::string> writeProject(const Project& project) {
    std::ostringstream out;
    out << "whitehole-project " << kFormatVersion << "\n\n";
    out << "settings fps " << formatDouble(project.framesPerSecond) << " normalize "
        << nameOf(detail::kNormalizeModes, project.normalize) << " sacn-priority "
        << static_cast<unsigned>(project.sacnPriority) << " source-name " << quoteWord(project.sourceName) << "\n\n";

    for (std::size_t i = 0; i < project.destinations.size(); ++i) {
        const output::Destination& d = project.destinations[i];
        out << "destination " << i << " " << (d.address.empty() ? "multicast" : d.address);
        if (d.port != 0) {
            out << " " << d.port;
        }
        out << "\n";
    }
    out << "\n";

    std::size_t patched = 0;
    for (const mapping::Fixture& fixture : project.layout.fixtures()) {
        out << "fixture " << quoteWord(fixture.name) << " at " << vec3(fixture.origin) << "\n";
        writeShape(out, fixture.shape);
        for (const output::FixturePatch& p : project.patches) {
            if (p.fixture == fixture.id) {
                writePatch(out, p);
                ++patched;
            }
        }
        out << "end\n\n";
    }
    if (patched != project.patches.size()) {
        return Error{ErrorCode::InvalidArgument, "Ein Patch verweist auf ein Fixture, das nicht im Layout steht"};
    }

    if (const Result<void> graph = writeGraph(out, project.graph, project.target, "Das Ziel (output)"); !graph.ok()) {
        return graph.error();
    }
    for (const Look& look : project.looks) {
        out << "\nlook " << quoteWord(look.name) << " fade " << formatDouble(look.fadeSeconds) << "\n";
        if (const Result<void> graph = writeGraph(out, look.graph, look.target, "Das Ziel des Looks"); !graph.ok()) {
            return graph.error();
        }
        out << "endlook\n";
    }
    return out.str();
}

}  // namespace whitehole::project
