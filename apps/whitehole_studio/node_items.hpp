// node_items.hpp: die Zeichenelemente des Node-Editors (Nodes und Kabel).
//
// Warum es diese Datei gibt: Der Editor (node_editor.cpp) kümmert sich um Mausbedienung und
// Änderungen am Graphen; wie ein Node und ein Kabel aussehen und wo ihre Ports liegen, steht hier.
// Die Items kennen weder Player noch Graph-Änderungen, nur die Beschreibung eines Node-Typs.
//
// Ein NodeItem zeigt Titel und Ports (Eingänge links, Ausgänge rechts). Parameter stehen nicht im
// Node, sondern im Dock "Parameter".
#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QPainter>
#include <QString>

#include <functional>
#include <optional>
#include <utility>
#include <string>
#include <vector>

#include "whitehole/graph/graph_model.hpp"

namespace whitehole::studio {

/// Farbe eines Porttyps; Kabel und Ports zeigen sie, damit man Verbindbares erkennt. Die zwölf Farben sind
/// über den Farbkreis verteilt und auf dunklem Grund gut zu unterscheiden.
[[nodiscard]] QColor portColor(graph::PortType type);

/// Form des Port-Zeichens: Zusätzlich zur Farbe, damit auch ähnliche Farben (oder Farbsehschwäche) nicht
/// verwechselt werden. Kreis: Felder, Bilder, Zahlen, Farben; Raute: Ganzzahl, Schalter, Trigger;
/// Quadrat: Text, Punktwolke; Dreieck: Audio.
enum class PortShape { Circle, Diamond, Square, Triangle };
[[nodiscard]] PortShape portShape(graph::PortType type);

/// Zeichnet ein Port-Zeichen. Ausgänge sind gefüllt, Eingänge nur ein farbiger Ring mit dunkler Mitte
/// ("Steckdose"), so unterscheiden sie sich auch bei gleicher Farbe.
void drawPortMarker(QPainter* painter, const QPointF& center, graph::PortType type, bool output, double radius);

/// Selbsttest für `--exercise`: leer, wenn alle Typen unterscheidbar sind (Farbe oder Form), sonst die Beschreibung.
[[nodiscard]] QString checkPortStyles();

struct PortSlot {
    std::string key;
    QString label;
    graph::PortType type{graph::PortType::Field};
    bool output{false};
    QPointF anchor;  // Mitte des Port-Kreises in Item-Koordinaten
};

class NodeItem : public QGraphicsItem {
public:
    NodeItem(NodeId id, const graph::NodeDescriptor& descriptor, const QString& title);

    [[nodiscard]] NodeId nodeId() const noexcept { return id_; }
    [[nodiscard]] const std::vector<PortSlot>& ports() const noexcept { return ports_; }
    [[nodiscard]] const PortSlot* findPort(const std::string& key) const;
    /// Der Port unter einem Punkt in Szenenkoordinaten, sonst nullptr.
    [[nodiscard]] const PortSlot* portAt(const QPointF& scenePos) const;
    [[nodiscard]] QPointF anchorScene(const PortSlot& port) const { return mapToScene(port.anchor); }

    /// Markiert den Ausgang, der auf die LEDs abgetastet wird (Orange).
    void setTargetKey(std::optional<std::string> key);
    /// Meldet Verschiebungen (für die Kabel). Schaltet die Meldung ein; erst nach setPos aufrufen,
    /// damit die Startposition nicht als Verschiebung zählt.
    void setMoveCallback(std::function<void(NodeItem&)> callback);
    /// Wurde der Node seit dem letzten Aufruf verschoben? Setzt den Merker zurück.
    bool takeMoved() { return std::exchange(moved_, false); }

    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    NodeId id_;
    QString title_;
    QColor titleColor_;
    std::vector<PortSlot> ports_;
    double height_{0.0};
    std::optional<std::string> targetKey_;
    std::function<void(NodeItem&)> onMoved_;
    bool moved_{false};
};

class WireItem : public QGraphicsPathItem {
public:
    WireItem(ConnectionId id, NodeItem* from, std::string fromKey, NodeItem* to, std::string toKey,
             graph::PortType type);

    [[nodiscard]] ConnectionId connectionId() const noexcept { return id_; }
    [[nodiscard]] const NodeItem* fromNode() const noexcept { return from_; }
    [[nodiscard]] const NodeItem* toNode() const noexcept { return to_; }
    /// Zieht das Kabel an die aktuellen Port-Positionen nach.
    void updatePath();

    /// Weicher Verlauf von `a` (Ausgang, Kabel läuft nach rechts) nach `b` (Eingang).
    [[nodiscard]] static QPainterPath curve(const QPointF& a, const QPointF& b);

    [[nodiscard]] QRectF boundingRect() const override;
    [[nodiscard]] QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    ConnectionId id_;
    NodeItem* from_;
    NodeItem* to_;
    std::string fromKey_;
    std::string toKey_;
    QColor color_;
};

}  // namespace whitehole::studio
