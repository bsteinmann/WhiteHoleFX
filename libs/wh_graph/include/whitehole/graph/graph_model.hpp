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

/// Obergrenze für Faktor und Offset einer Verbindung (verhindert Überläufe im Parameterpuffer).
inline constexpr double kMaxConnectionScale = 1.0e9;

struct Connection {
    ConnectionId id;
    PortRef from;  // Ausgang
    PortRef to;    // Eingang
    /// Skalierung am Kabel: Landet ein Zahlenwert in einem Zahlenparameter, wird er als `wert * scale + offset`
    /// übergeben (vor dem Begrenzen auf den Bereich des Parameters). 1 und 0 lassen den Wert unverändert.
    /// So steuert ein Wert von 0 bis 1 etwa einen Winkel von 0 bis 360 Grad.
    double scale{1.0};
    double offset{0.0};

    [[nodiscard]] bool isScaled() const noexcept { return scale != 1.0 || offset != 0.0; }
};

struct NodeInstance {
    NodeId id;
    std::string typeId;
    std::string label;
    Vec2 position;  // Lage im Node-Editor
    std::unordered_map<std::string, ParameterValue> parameters;
    /// Überbrückt: Der Node ist noch da, wirkt aber nicht. Er reicht seinen ersten Field-Eingang unverändert
    /// weiter; ohne verbundenen Field-Eingang (Generatoren) liefert er Transparent. Nur für Field-Nodes.
    bool bypassed{false};
    /// Ziele eines Output-Nodes (mehrere Outputs): Einträge "fixture:<Name>", "group:<Name>" oder "region:<Name>".
    /// Leer = der Output bespielt, was seine Parameter (Rechteck) sagen. Das Graph-Modell kennt die Bedeutung
    /// nicht; sie legt das Projekt fest (project/output_target.hpp).
    std::vector<std::string> targets;
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

    /// Setzt Faktor und Offset einer Verbindung (siehe Connection::scale). Beide müssen endlich sein und
    /// betragsmäßig höchstens kMaxConnectionScale. Fehler: NotFound, InvalidArgument.
    Result<void> setConnectionScale(ConnectionId id, double scale, double offset);

    /// Der Wert muss denselben Typ haben wie der Standardwert des Parameters.
    /// Zahlen werden auf [minValue, maxValue] begrenzt; NaN und Unendlich
    /// werden abgelehnt (sie würden später im GPU-Uniform landen).
    /// Fehler: NotFound (Node oder Parameter), InvalidArgument (falscher Typ
    /// oder nicht endliche Zahl).
    Result<void> setParameter(NodeId id, std::string_view key, ParameterValue value);

    /// Schaltet die Überbrückung eines Field-Nodes (siehe NodeInstance::bypassed).
    /// Fehler: NotFound, Unsupported (kein Field-Node, etwa ein CPU-Wert-Node).
    Result<void> setBypassed(NodeId id, bool bypassed);

    /// Setzt die Ziele eines Nodes (siehe NodeInstance::targets); Duplikate werden entfernt, die Reihenfolge bleibt.
    /// Fehler: NotFound, InvalidArgument (leerer Eintrag).
    Result<void> setTargets(NodeId id, std::vector<std::string> targets);

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

    /// Strenger Modus (Issue #25): Aus (Standard) gilt ein unverbundener Eingang eines Blend-Nodes als durchsichtiges
    /// Schwarz; an ist er wie früher ein Pflicht-Eingang. Gilt für Kompilierung und Vollständigkeitsprüfung.
    void setStrictInputs(bool strict) noexcept { strictInputs_ = strict; }
    [[nodiscard]] bool strictInputs() const noexcept { return strictInputs_; }

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
    bool strictInputs_{false};
};

}  // namespace whitehole::graph
