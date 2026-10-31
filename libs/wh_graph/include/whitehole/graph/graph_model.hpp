// graph_model.hpp: Datenmodell des Effektgraphen.
// Graph ist ein Wertetyp: Eine Kopie ist ein unveränderlicher Snapshot, den
// Engine und GUI gefahrlos nebenläufig lesen können. Hier stehen nur die
// Deklarationen; die Implementierung samt Zyklenprüfung folgt in Schritt 4.
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/graph/node_descriptor.hpp"
#include "whitehole/graph/node_registry.hpp"

namespace whitehole::graph {

/// Adressiert einen Port eines Nodes über dessen stabilen Schlüssel.
struct PortRef {
    NodeId node;
    std::string portKey;

    bool operator==(const PortRef&) const = default;
};

struct Connection {
    ConnectionId id;
    PortRef from;  // Ausgang
    PortRef to;    // Eingang
};

struct NodeInstance {
    NodeId id;
    std::string typeId;
    std::string label;
    Vec2 position;  // Lage im Node-Editor
    std::unordered_map<std::string, ParameterValue> parameters;
};

class Graph {
public:
    explicit Graph(std::shared_ptr<const INodeRegistry> registry);

    /// Legt einen Node mit den Standardwerten seiner Parameter an.
    /// Fehler: NotFound, wenn der Typ nicht registriert ist.
    [[nodiscard]] Result<NodeId> addNode(std::string_view typeId, Vec2 position);

    /// Entfernt den Node samt aller Verbindungen.
    Result<void> removeNode(NodeId id);

    /// Prüft: beide Nodes und Ports existieren, Richtung Ausgang -> Eingang,
    /// isConnectable() erfüllt, kein Zyklus und der Eingang ist noch frei.
    /// Fehler: NotFound, IncompatiblePorts, CycleDetected, AlreadyExists
    /// (Eingang belegt). Eine bestehende Verbindung wird nie stillschweigend
    /// ersetzt: Der Editor löst sie zuerst und verbindet danach neu.
    [[nodiscard]] Result<ConnectionId> connect(const PortRef& from, const PortRef& to);

    Result<void> disconnect(ConnectionId id);

    /// Der Wert muss denselben Typ haben wie der Standardwert des Parameters.
    /// Zahlen werden auf [minValue, maxValue] begrenzt; NaN und Unendlich
    /// werden abgelehnt (sie würden später im GPU-Uniform landen).
    /// Fehler: NotFound (Node oder Parameter), InvalidArgument (falscher Typ
    /// oder nicht endliche Zahl).
    Result<void> setParameter(NodeId id, std::string_view key, ParameterValue value);

    /// Lage im Node-Editor. Hat keinen Einfluss auf das Ergebnis des Graphen.
    /// Fehler: NotFound, InvalidArgument (NaN oder Unendlich).
    Result<void> setPosition(NodeId id, Vec2 position);

    [[nodiscard]] const NodeInstance* findNode(NodeId id) const noexcept;
    [[nodiscard]] const Connection* findConnection(ConnectionId id) const noexcept;

    /// Die Verbindung, die in den angegebenen Eingang führt, oder nullptr.
    [[nodiscard]] const Connection* findInputConnection(NodeId node,
                                                        std::string_view portKey) const noexcept;

    /// Das Verzeichnis, gegen das dieser Graph prüft (z. B. für den Compiler).
    [[nodiscard]] const INodeRegistry& registry() const noexcept { return *registry_; }
    /// Dasselbe Verzeichnis als geteilter Zeiger (für Graphen, die dazu passen müssen, etwa die Looks).
    [[nodiscard]] std::shared_ptr<const INodeRegistry> registryPtr() const noexcept { return registry_; }

    [[nodiscard]] const std::vector<NodeInstance>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<Connection>& connections() const noexcept {
        return connections_;
    }

private:
    std::shared_ptr<const INodeRegistry> registry_;
    std::vector<NodeInstance> nodes_;
    std::vector<Connection> connections_;
    IdGenerator<NodeId> nodeIds_;
    IdGenerator<ConnectionId> connectionIds_;
};

}  // namespace whitehole::graph
