// node_editor.hpp: Ansicht zum Bearbeiten des Effektgraphen.
//
// Was man tun kann:
//   Node hinzufügen      Rechtsklick auf freie Fläche, Menü nach Kategorien
//   verschieben          Node ziehen (die Lage wird in der Projektdatei gespeichert)
//   verbinden            von einem Port zu einem passenden anderen ziehen; ein belegter Eingang
//                        wird ersetzt
//   löschen              Node oder Kabel anklicken, Entf (Mehrfachauswahl mit Strg oder Rahmen)
//   Ausgang wählen       Rechtsklick auf einen Field-Ausgang, "Als Ausgang setzen" (orange)
//   Ansicht              Mausrad zoomt, mittlere Taste verschiebt, Taste F zeigt alles
//
// Der Editor hält keinen eigenen Graphen: Jede Änderung geht über den Player, danach wird die
// Szene aus dem Graphen neu aufgebaut. So gibt es nur eine Wahrheit, und Fehler (Typ passt nicht,
// Zyklus) kommen als Text vom Player und erscheinen über `failed`.
#pragma once

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPointF>
#include <QString>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "node_items.hpp"
#include "whitehole/runtime/player.hpp"

class QMenu;

namespace whitehole::studio {

class NodeEditor : public QGraphicsView {
    Q_OBJECT

public:
    explicit NodeEditor(runtime::Player& player, QWidget* parent = nullptr);

    /// Baut die Szene neu auf und zeigt alle Nodes (nach dem Laden eines Projekts).
    void reset();
    /// Baut die Szene neu auf, die Ansicht bleibt.
    void rebuild();
    /// Zoomt so, dass alle Nodes zu sehen sind (höchstens im Maßstab 1:1).
    void showAll();

    /// Legt einen Node an; meldet Fehler über `failed`.
    std::optional<NodeId> addNodeAt(const std::string& typeId, const QPointF& scenePos);
    /// Verbindet Ausgang `from` mit Eingang `to`. Meldet Fehler über `failed`.
    bool connectPorts(const graph::PortRef& from, const graph::PortRef& to);
    /// Löscht die ausgewählten Nodes und Kabel.
    void removeSelected();
    /// Wählt den Ausgang, der auf die LEDs abgetastet wird.
    bool setTarget(const graph::PortRef& port);

    /// Rauchtest für die CI: legt zwei Nodes an, zieht ein Kabel zwischen ihnen auf dem Weg der
    /// Maus (Treffer auf Ports), löscht es und die Nodes wieder. Leer, wenn alles wie erwartet war,
    /// sonst eine Meldung, welcher Schritt scheiterte.
    QString exercise();

    [[nodiscard]] int nodeCount() const noexcept { return static_cast<int>(nodes_.size()); }
    [[nodiscard]] int wireCount() const noexcept { return static_cast<int>(wires_.size()); }

    [[nodiscard]] QSize sizeHint() const override { return {720, 300}; }

Q_SIGNALS:
    /// Der Graph hat sich geändert. `structural`: Nodes oder Kabel (das Parameterdock muss neu
    /// aufgebaut werden); sonst nur Positionen.
    void edited(bool structural);
    void failed(const QString& message);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Hit {
        NodeItem* node{nullptr};
        const PortSlot* port{nullptr};
    };

    [[nodiscard]] Hit portAt(const QPointF& scenePos) const;
    [[nodiscard]] NodeItem* nodeAt(const QPointF& scenePos) const;
    [[nodiscard]] WireItem* wireAt(const QPointF& scenePos) const;
    [[nodiscard]] NodeItem* itemOf(NodeId id) const;
    void fail(const std::string& message);
    void commitMoves();

    // Kabel ziehen (Maus und exercise() benutzen dieselben Schritte)
    bool beginWire(const QPointF& scenePos);
    void moveWire(const QPointF& scenePos);
    bool endWire(const QPointF& scenePos);
    void cancelWire();

    void buildAddMenu(QMenu& menu, const QPointF& scenePos);

    runtime::Player& player_;
    QGraphicsScene scene_;
    std::unordered_map<std::uint64_t, NodeItem*> nodes_;
    std::vector<WireItem*> wires_;

    NodeItem* wireNode_{nullptr};
    PortSlot wireSlot_;
    QGraphicsPathItem* tempWire_{nullptr};

    bool panning_{false};
    QPoint panLast_;
    bool fitPending_{false};
};

}  // namespace whitehole::studio
