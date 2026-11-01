// node_editor.hpp: Ansicht zum Bearbeiten des Effektgraphen.
//
// Was man tun kann:
//   Node hinzufügen      Rechtsklick auf freie Fläche, Menü nach Kategorien; oder Strg+Leertaste
//                        und tippen (Suchfeld); oder aus der Node-Palette hereinziehen
//   Node entfernen       Entf, oder den Node auf die Node-Palette ziehen und loslassen
//   verschieben          Node ziehen (die Lage wird in der Projektdatei gespeichert)
//   verbinden            von einem Port zu einem passenden anderen ziehen; ein belegter Eingang
//                        wird ersetzt
//   löschen              Node oder Kabel anklicken, Entf (Mehrfachauswahl mit Strg oder Rahmen)
//   kopieren             Strg+C, einfügen Strg+V (an der Maus), duplizieren Strg+D; mit den Kabeln
//                        zwischen den ausgewählten Nodes, ein Schritt für Rückgängig
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
#include "whitehole/runtime/node_preset.hpp"
#include "whitehole/runtime/player.hpp"

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QEvent;
class QMenu;

namespace whitehole::studio {

class NodeEditor : public QGraphicsView {
    Q_OBJECT

public:
    explicit NodeEditor(runtime::Player& player, QWidget* parent = nullptr);
    ~NodeEditor() override;

    /// Baut die Szene neu auf und zeigt alle Nodes (nach dem Laden eines Projekts).
    void reset();
    /// Baut die Szene neu auf, die Ansicht bleibt.
    void rebuild();
    /// Zoomt so, dass alle Nodes zu sehen sind (höchstens im Maßstab 1:1).
    void showAll();

    /// Kopieren, Einfügen, Duplizieren (Strg+C, Strg+V, Strg+D). Eingefügt wird an `scenePos`
    /// (linke obere Ecke der Kopie) oder, ohne Angabe, an der Maus (sonst leicht versetzt).
    /// Die Kopie ist danach ausgewählt. Wahr, wenn etwas eingefügt wurde.
    bool copySelection();
    bool paste(const std::optional<QPointF>& scenePos = std::nullopt);
    bool duplicateSelection();
    [[nodiscard]] bool canPaste() const noexcept { return !clipboard_.empty(); }

    /// Presets: ausgewählte Nodes samt Kabeln als Datei merken und später als normale Nodes einfügen
    /// (siehe runtime/node_preset.hpp). Der Ordner gehört dem Aufrufer und muss den Editor überleben;
    /// ohne Ordner gibt es die Menüpunkte nicht.
    void setPresetStore(const runtime::PresetStore* store) noexcept { presets_ = store; }
    /// Speichert die ausgewählten Nodes unter `name` (ersetzt ein gleichnamiges Preset). Meldet Fehler
    /// über `failed`, Erfolg über `notice`.
    bool savePreset(const QString& name);
    /// Fragt nach einem Namen (und beim Ersetzen nach Bestätigung), dann savePreset.
    bool savePresetInteractive();
    /// Fügt das Preset mit der linken oberen Ecke an `scenePos` ein; die Kopie ist danach ausgewählt.
    bool insertPreset(const QString& name, const QPointF& scenePos);
    /// Löscht ein Preset (ohne Rückfrage; die fragt das Menü).
    bool deletePreset(const QString& name);

    /// Der Node, der als einziges ausgewählt ist (leer bei keiner oder mehreren Auswahlen).
    [[nodiscard]] std::optional<NodeId> selectedNode() const;
    /// Wählt genau diesen Node aus (und sonst nichts); wahr, wenn es ihn gibt.
    bool selectNode(NodeId id);

    /// Legt einen Node in der Mitte der sichtbaren Fläche an (Doppelklick in der Palette).
    std::optional<NodeId> addNodeAtCenter(const std::string& typeId);
    /// Das Widget, auf dem losgelassene Nodes gelöscht werden (die Palette). Gehört dem Aufrufer.
    void setDeleteZone(QWidget* zone);

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
    /// Hinweis, der kein Fehler ist (z. B. der Ausgang ist umgezogen).
    void notice(const QString& message);
    /// Die Auswahl hat sich durch Anklicken oder selectNode geändert (nicht durch Neuaufbau).
    void selectionChanged();
    /// Ein Node wird gerade über die Löschzone gezogen (true) oder wieder davon weg (false).
    void deleteZoneHover(bool over);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    /// Fängt Drags mit Node-Typ schon am Viewport ab (dorthin liefert Qt die Drops; QGraphicsView leitet sie sonst an die Szene).
    bool viewportEvent(QEvent* event) override;
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
    [[nodiscard]] bool overDeleteZone(const QPoint& globalPos) const;

    // Kabel ziehen (Maus und exercise() benutzen dieselben Schritte)
    bool beginWire(const QPointF& scenePos);
    void moveWire(const QPointF& scenePos);
    bool endWire(const QPointF& scenePos);
    void cancelWire();

    void buildAddMenu(QMenu& menu, const QPointF& scenePos);
    /// Untermenüs "Preset einfügen" und "Preset löschen" (nur mit Preset-Ordner).
    void buildPresetMenus(QMenu& menu, const QPointF& scenePos);
    /// Suchfeld zum Hinzufügen eines Nodes an `scenePos`.
    void showSearch(const QPointF& scenePos);
    /// Baut die Szene aus dem Graphen auf (ohne Auswahl zu retten, siehe rebuild).
    void buildScene();

    [[nodiscard]] std::vector<NodeId> selectedNodes() const;
    bool pasteWithOffset(const runtime::NodeClipboard& clip, const QPointF& offset);

    runtime::Player& player_;
    runtime::NodeClipboard clipboard_;
    const runtime::PresetStore* presets_{nullptr};
    QGraphicsScene scene_;
    std::unordered_map<std::uint64_t, NodeItem*> nodes_;
    std::vector<WireItem*> wires_;

    NodeItem* wireNode_{nullptr};
    PortSlot wireSlot_;
    QGraphicsPathItem* tempWire_{nullptr};

    QWidget* deleteZone_{nullptr};
    bool nodeDragging_{false};  // die linke Maustaste ist auf einem Node gedrückt (Verschieben läuft)
    bool overZone_{false};
    bool rebuilding_{false};  // während des Neuaufbaus kein selectionChanged melden
    bool panning_{false};
    QPoint panLast_;
    bool fitPending_{false};
};

}  // namespace whitehole::studio
