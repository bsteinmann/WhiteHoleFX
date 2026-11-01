// player_edit.cpp: Graph bearbeiten (Teil des Players, ausgelagert wegen der Dateigröße).
#include "whitehole/runtime/player.hpp"

#include <unordered_set>
#include <vector>

namespace whitehole::runtime {

namespace {

Error noProject() { return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"}; }

/// Der Field-Ausgang, der in `node` hineinläuft (erste Verbindung in Reihenfolge des Graphen).
std::optional<graph::PortRef> upstreamFieldOutput(const graph::Graph& graph, NodeId node) {
    for (const graph::Connection& c : graph.connections()) {
        if (c.to.node != node || c.from.node == node) {
            continue;
        }
        const graph::NodeInstance* source = graph.findNode(c.from.node);
        const graph::NodeDescriptor* descriptor = source ? graph.registry().find(source->typeId) : nullptr;
        const graph::PortDescriptor* port = descriptor ? descriptor->findPort(c.from.portKey) : nullptr;
        if (port != nullptr && port->type == graph::PortType::Field) {
            return c.from;
        }
    }
    return std::nullopt;
}

/// Eine Verbindung, die nach dem Löschen eines Nodes zwischen seinen Nachbarn neu entsteht.
struct Bridge {
    graph::PortRef from;
    graph::PortRef to;
};

/// Was beim Löschen von `node` überbrückt wird: Für jeden Ausgang des Nodes mit Verbindungen sucht sich der Node
/// den ersten Eingang gleichen Typs, der belegt ist (echte Eingänge vor Modulationseingängen von Parametern). Dessen
/// Quelle wird mit allen Zielen des Ausgangs verbunden. So bleibt aus A -> X -> B nach dem Löschen von X ein A -> B.
std::vector<Bridge> bridgesAround(const graph::Graph& graph, NodeId node) {
    std::vector<Bridge> bridges;
    const graph::NodeInstance* instance = graph.findNode(node);
    const graph::NodeDescriptor* descriptor = instance != nullptr ? graph.registry().find(instance->typeId) : nullptr;
    if (descriptor == nullptr) {
        return bridges;
    }
    for (const graph::PortDescriptor& out : descriptor->ports) {
        if (out.direction != graph::PortDirection::Output) {
            continue;
        }
        std::vector<graph::PortRef> targets;
        for (const graph::Connection& c : graph.connections()) {
            if (c.from.node == node && c.from.portKey == out.key && c.to.node != node) {
                targets.push_back(c.to);
            }
        }
        if (targets.empty()) {
            continue;
        }
        const graph::Connection* chosen = nullptr;
        for (const bool modulation : {false, true}) {
            for (const graph::PortDescriptor& in : descriptor->ports) {
                if (chosen != nullptr || in.direction != graph::PortDirection::Input || in.type != out.type ||
                    (descriptor->findParameter(in.key) != nullptr) != modulation) {
                    continue;
                }
                const graph::Connection* incoming = graph.findInputConnection(node, in.key);
                if (incoming != nullptr && incoming->from.node != node) {
                    chosen = incoming;
                }
            }
        }
        if (chosen == nullptr) {
            continue;
        }
        for (const graph::PortRef& target : targets) {
            bridges.push_back(Bridge{chosen->from, target});
        }
    }
    return bridges;
}

}  // namespace

bool Player::targetIncomplete() const {
    if (!project_) {
        return false;
    }
    const graph::Graph& graph = activeGraph();
    const std::optional<graph::PortRef> target = live_ ? std::optional<graph::PortRef>{live_->target()} : project_->target;
    if (!target) {
        return false;
    }
    // Rückwärts vom Ausgang: jeder erreichte Node muss alle Pflicht-Eingänge belegt haben.
    std::vector<NodeId> open{target->node};
    std::unordered_set<std::uint64_t> seen;
    while (!open.empty()) {
        const NodeId id = open.back();
        open.pop_back();
        if (!seen.insert(id.value()).second) {
            continue;
        }
        const graph::NodeInstance* instance = graph.findNode(id);
        const graph::NodeDescriptor* descriptor = instance != nullptr ? graph.registry().find(instance->typeId) : nullptr;
        if (descriptor == nullptr) {
            continue;
        }
        for (const graph::PortDescriptor& port : descriptor->ports) {
            if (port.direction != graph::PortDirection::Input) {
                continue;
            }
            const graph::Connection* connection = graph.findInputConnection(id, port.key);
            if (connection == nullptr) {
                if (port.required) {
                    return true;
                }
                continue;
            }
            open.push_back(connection->from.node);
        }
    }
    return false;
}

void Player::refreshCompleteness() {
    incomplete_ = targetIncomplete();
    applyOutputLevel();
}

void Player::applyOutputLevel() {
    if (session_) {
        session_->setOutputLevel(master_, blackout_ || incomplete_);
    }
}

Result<NodeId> Player::addNode(std::string_view typeId, Vec2 position) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<NodeId> id = project_->graph.addNode(typeId, position);
    if (id.ok()) {
        // Eigene Standardwerte: Typ und Bereich prüft der Graph; Unpassendes bleibt beim Bibliothekswert.
        if (const NodeDefaults::Values* own = nodeDefaults_.valuesOf(std::string{typeId})) {
            for (const auto& [key, value] : *own) {
                (void)project_->graph.setParameter(id.value(), key, value);
            }
        }
    }
    if (id.ok() && typeId == "wh.output.canvas") {
        // Ein neuer Ausgangs-Node wird zum Ausgang des Projekts und legt sich über den bisherigen: Dessen Bild
        // kommt an `behind` (so bleibt er außerhalb des neuen Rechtecks sichtbar) und zunächst auch an `source`
        // (damit der Graph sofort gültig ist; man schließt dort den eigenen Effekt an). Die Eingänge eines neuen
        // Nodes sind frei, die Verbindungen gelingen immer.
        if (project_->target) {
            (void)project_->graph.connect(*project_->target, graph::PortRef{id.value(), "source"});
            (void)project_->graph.connect(*project_->target, graph::PortRef{id.value(), "behind"});
        }
        project_->target = graph::PortRef{id.value(), "out"};
        rebuildDirty_ = true;
    }
    finishEdit(std::move(before), {}, id.ok());
    if (id.ok()) {
        modified_ = true;
        graphDirty_ = true;
    }
    return id;
}

Result<void> Player::removeNode(NodeId node) {
    if (!project_) {
        return noProject();
    }
    // Liefert der Node den Ausgang, rückt der Ausgang auf den Node davor (den ersten Field-Eingang,
    // der belegt ist). Gibt es keinen, bleibt das Projekt ohne Ausgang; das Starten meldet es dann.
    const bool wasTarget = project_->target && project_->target->node == node;
    std::optional<graph::PortRef> upstream;
    if (wasTarget) {
        upstream = upstreamFieldOutput(project_->graph, node);
    }
    // Aus A -> X -> B wird beim Löschen von X ein A -> B (siehe bridgesAround). Fehlschläge (Typ, Zyklus) sind kein Fehler.
    const std::vector<Bridge> bridges = bridgesAround(project_->graph, node);
    auto before = beginEdit({});
    const Result<void> r = project_->graph.removeNode(node);
    if (r.ok()) {
        for (const Bridge& bridge : bridges) {
            (void)project_->graph.connect(bridge.from, bridge.to);
        }
    }
    if (r.ok() && wasTarget) {
        project_->target = upstream;
    }
    finishEdit(std::move(before), {}, r.ok());
    if (r.ok()) {
        modified_ = true;
        graphDirty_ = true;
        if (wasTarget) {
            rebuildDirty_ = true;  // das Ziel steckt in der Stage
        }
    }
    return r;
}

Result<ConnectionId> Player::connect(const graph::PortRef& from, const graph::PortRef& to) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    graph::Graph& g = project_->graph;
    // Eine belegte Eingabe wird ersetzt. Die alte Verbindung merken wir uns, damit ein
    // Fehlschlag (Typ, Zyklus) nichts zerstört.
    std::optional<graph::Connection> previous;
    if (const graph::Connection* existing = g.findInputConnection(to.node, to.portKey)) {
        previous = *existing;
        (void)g.disconnect(existing->id);
    }
    Result<ConnectionId> id = g.connect(from, to);
    if (!id.ok()) {
        if (previous) {
            (void)g.connect(previous->from, previous->to);
        }
        return id;
    }
    finishEdit(std::move(before), {}, true);
    modified_ = true;
    graphDirty_ = true;
    return id;
}

Result<void> Player::disconnect(ConnectionId connection) {
    if (!project_) {
        return noProject();
    }
    auto before = beginEdit({});
    const Result<void> r = project_->graph.disconnect(connection);
    finishEdit(std::move(before), {}, r.ok());
    if (r.ok()) {
        modified_ = true;
        graphDirty_ = true;
    }
    return r;
}

Result<void> Player::moveNode(NodeId node, Vec2 position) {
    if (!project_) {
        return noProject();
    }
    const std::string editKey = "move:" + std::to_string(node.value());
    auto before = beginEdit(editKey);
    const Result<void> r = project_->graph.setPosition(node, position);
    finishEdit(std::move(before), editKey, r.ok());
    if (r.ok()) {
        modified_ = true;
    }
    return r;
}

Result<void> Player::setTarget(const graph::PortRef& port) {
    if (!project_) {
        return noProject();
    }
    const graph::NodeInstance* instance = project_->graph.findNode(port.node);
    if (instance == nullptr) {
        return Error{ErrorCode::NotFound, "Node existiert nicht"};
    }
    const graph::NodeDescriptor* descriptor = project_->graph.registry().find(instance->typeId);
    const graph::PortDescriptor* found = descriptor ? descriptor->findPort(port.portKey) : nullptr;
    if (found == nullptr) {
        return Error{ErrorCode::NotFound, "Port existiert nicht: " + port.portKey};
    }
    if (found->direction != graph::PortDirection::Output || found->type != graph::PortType::Field) {
        return Error{ErrorCode::IncompatiblePorts, "Als Ausgang taugt nur ein Field-Ausgang"};
    }
    if (project_->target && *project_->target == port) {
        return {};
    }
    auto before = beginEdit({});
    project_->target = port;
    finishEdit(std::move(before), {}, true);
    rebuildDirty_ = true;
    modified_ = true;
    return {};
}

DmxUsage Player::dmxUsage() const { return project_ ? findDmxUsage(project_->graph) : DmxUsage{}; }

AudioUsage Player::audioUsage() const { return project_ ? findAudioUsage(project_->graph) : AudioUsage{}; }

}  // namespace whitehole::runtime
