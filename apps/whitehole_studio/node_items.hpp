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
#include <QGraphicsSceneHoverEvent>
#include <QPainter>
#include <QSizeF>
#include <QString>

#include <cstdint>
#include <array>
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

/// Maße eines Nodes; die Kachel einer Gruppe (node_items_group.cpp) legt ihre Ports genauso an.
inline constexpr double kNodeWidth = 190.0;
inline constexpr double kNodeTitleHeight = 26.0;
inline constexpr double kNodeRowHeight = 22.0;

/// Was ein Ausgang gerade liefert (Live-Anzeige): ein Pegelbalken, 32 Balken (Spektrum, Welle) oder eine Farbe.
/// `level` (0..1) gilt für alle Arten und färbt auch das Kabel.
struct LiveDisplay {
    enum class Kind { None, Level, Bars, Color };
    Kind kind{Kind::None};
    float level{0.0F};
    std::array<float, 32> bars{};
    QColor color;
    bool operator==(const LiveDisplay&) const = default;
};

struct PortSlot {
    std::string key;
    QString label;
    graph::PortType type{graph::PortType::Field};
    bool output{false};
    QPointF anchor;  // Mitte des Port-Kreises in Item-Koordinaten
    /// Nur bei der Kachel einer Gruppe: der echte Port eines Mitglieds, für den dieser Port steht (node_group.hpp).
    /// `realNode` 0 = gewöhnlicher Port des Items selbst.
    std::uint64_t realNode{0};
    std::string realKey;
    LiveDisplay live;
};

/// Zeichnet die Live-Anzeige eines Ausgangs (Balken, Spektrum, Farbstreifen) unter oder hinter seinem Namen;
/// `width` ist die Breite des Namensfelds. Eingänge und Ausgänge ohne Live-Wert zeichnet es nicht.
void drawPortLive(QPainter* painter, const PortSlot& port, double width);

class NodeItem : public QGraphicsItem {
public:
    NodeItem(NodeId id, const graph::NodeDescriptor& descriptor, const QString& title);
    /// Kachel einer zugeklappten Gruppe (node_items_group.cpp): `id` ist der erste Node der Gruppe, `ports` kommen ohne
    /// Lage (die Anker setzt der Konstruktor). Bypass und Info gibt es nicht.
    NodeItem(NodeId id, const QString& groupName, std::vector<PortSlot> ports, const QColor& color);

    [[nodiscard]] bool isGroupTile() const noexcept { return groupTile_; }
    /// Der Port im Graphen, für den `slot` steht (bei einem gewöhnlichen Node der Port selbst).
    [[nodiscard]] graph::PortRef portRef(const PortSlot& slot) const;

    [[nodiscard]] NodeId nodeId() const noexcept { return id_; }
    /// Größe des Nodes (ohne Rand) in Szenenkoordinaten; `sizeFor` rechnet sie für einen Typ, der noch nicht im Editor steht.
    [[nodiscard]] QSizeF size() const;
    [[nodiscard]] static QSizeF sizeFor(const graph::NodeDescriptor& descriptor);
    [[nodiscard]] const std::vector<PortSlot>& ports() const noexcept { return ports_; }
    [[nodiscard]] const PortSlot* findPort(const std::string& key) const;
    /// Der Port unter einem Punkt in Szenenkoordinaten, sonst nullptr.
    [[nodiscard]] const PortSlot* portAt(const QPointF& scenePos) const;
    [[nodiscard]] QPointF anchorScene(const PortSlot& port) const { return mapToScene(port.anchor); }

    /// Überbrückung (Bypass): Field-Nodes haben im Kopf einen Schalter. Überbrückte Nodes erscheinen abgeblendet
    /// mit einer gestrichelten Durchleitung vom ersten Eingang zum Ausgang.
    [[nodiscard]] bool canBypass() const noexcept { return canBypass_; }
    [[nodiscard]] bool bypassed() const noexcept { return bypassed_; }
    void setBypassed(bool on);
    /// Liegt der Punkt (Szenenkoordinaten) auf dem Schalter im Kopf des Nodes?
    [[nodiscard]] bool bypassButtonAt(const QPointF& scenePos) const;

    /// Einrast-Vorschau beim Ziehen eines Nodes aus der Palette: der Node bekommt einen hellblauen Rahmen und der
    /// Port `portKey` (leer = keiner) einen Ring. Aus mit `on = false`.
    void setDockHighlight(bool on, std::string portKey = {});

    /// Info-Zeichen "i" im Kopf: Beschreibung des Node-Typs als Tooltip beim Überfahren; ein Klick zeigt ihn sofort.
    /// Ohne Text (leer) gibt es kein Zeichen.
    void setInfo(const QString& text);
    [[nodiscard]] const QString& info() const noexcept { return info_; }
    [[nodiscard]] const QString& title() const noexcept { return title_; }
    [[nodiscard]] bool infoButtonAt(const QPointF& scenePos) const;

    /// Live-Anzeige am Ausgang `portKey`: Balken im Node und Helligkeit des Kabels. Spitzen bleiben kurz stehen
    /// (fallen mit jedem Aufruf ab), damit ein einzelner Trigger-Frame sichtbar bleibt.
    void setLive(const std::string& portKey, LiveDisplay display);
    /// Nimmt die Live-Anzeige weg (Wiedergabe gestoppt oder Anzeige aus).
    void clearLive();
    /// Pegel 0..1 am Ausgang oder -1, wenn dort nichts Live anliegt.
    [[nodiscard]] float liveLevel(const std::string& portKey) const;

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
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    NodeId id_;
    QString title_;
    QColor titleColor_;
    std::vector<PortSlot> ports_;
    double height_{0.0};
    std::optional<std::string> targetKey_;
    std::function<void(NodeItem&)> onMoved_;
    bool moved_{false};
    bool canBypass_{false};
    bool bypassed_{false};
    bool groupTile_{false};
    bool dockHighlight_{false};
    std::string dockPort_;
    QString info_;
};

/// Rahmen um die Nodes einer aufgeklappten Gruppe (mit Namen). Nicht auswählbar und nicht beweglich: Er folgt den
/// Mitgliedern (`updateRect`) und liegt hinter ihnen.
class GroupFrameItem : public QGraphicsItem {
public:
    GroupFrameItem(QString name, const QColor& color, std::vector<NodeItem*> members);

    [[nodiscard]] const QString& name() const noexcept { return name_; }
    /// Passt den Rahmen an die Lage der Mitglieder an.
    void updateRect();

    [[nodiscard]] QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    QString name_;
    QColor color_;
    std::vector<NodeItem*> members_;
    QRectF rect_;
};

class WireItem : public QGraphicsPathItem {
public:
    WireItem(ConnectionId id, NodeItem* from, std::string fromKey, NodeItem* to, std::string toKey,
             graph::PortType type);

    [[nodiscard]] ConnectionId connectionId() const noexcept { return id_; }
    [[nodiscard]] const NodeItem* fromNode() const noexcept { return from_; }
    [[nodiscard]] const NodeItem* toNode() const noexcept { return to_; }
    [[nodiscard]] const std::string& fromKey() const noexcept { return fromKey_; }
    /// Pegel der Live-Anzeige 0..1 oder -1 (nichts Live).
    [[nodiscard]] float liveLevel() const noexcept { return live_; }
    /// Zieht das Kabel an die aktuellen Port-Positionen nach.
    void updatePath();
    /// Einrast-Vorschau: hellblau und dick, weil ein gezogener Node hier eingefügt würde.
    void setDockHighlight(bool on);
    [[nodiscard]] static QColor dockColor() { return QColor{90, 210, 255}; }
    /// Liest den Pegel vom Ausgang (NodeItem::liveLevel) und zeichnet neu, wenn er sich geändert hat.
    void syncLive();

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
    bool dockHighlight_{false};
    void paintLive(QPainter* painter) const;
    float live_{-1.0F};  // Pegel des Ausgangs 0..1, -1 = nichts Live
};

}  // namespace whitehole::studio
