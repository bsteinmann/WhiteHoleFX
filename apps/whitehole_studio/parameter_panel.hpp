// parameter_panel.hpp: Regler für alle Parameter eines Projekts.
//
// Für jeden Node mit Parametern eine Gruppe, darin je Parameter eine Zeile mit Namen, passendem
// Bedienelement und einem Knopf zum Zurücksetzen auf den Standardwert:
//   Zahl mit Bereich   Schieberegler und Zahlenfeld
//   Zahl ohne Bereich  Zahlenfeld
//   Ganzzahl           Zahlenfeld
//   Ja/Nein            Kästchen
//   2D-/3D-Wert        je Komponente ein Zahlenfeld
//   Farbe              Farbfläche (mit Schachbrett, damit Transparenz sichtbar ist), ein Klick öffnet
//                      die Farbwahl (Eingabe sRGB, gespeichert linear, mit Alpha-Regler); dazu ein
//                      Kästchen "Transparent" (Alpha 0)
//   Text               Eingabefeld
// Parameter, die eine Verbindung im Graphen steuert, sind ausgegraut.
// Wählt man im Node-Editor einen Node, hebt `showNode` seine Gruppe hervor und scrollt dorthin.
//
// Übersicht bei vielen Nodes (oben im Fenster): Ansicht "Gewählter Node" (Standard) zeigt nur die Gruppe des
// gewählten Nodes und der angehefteten Nodes; "Alle Nodes" zeigt jede Gruppe, aufgeklappt sind nur die des gewählten
// und der angehefteten Nodes (der Pfeil in jeder Gruppe klappt von Hand auf und zu). "Anheften" hält eine Gruppe im
// Fenster, auch wenn man im Editor weiterklickt (für Live-Regler wie Helligkeit oder Tempo). Das Suchfeld filtert
// nach Node- und Parameternamen über alle Nodes. Die Ansicht merkt sich das Programm (QSettings, parameters/mode).
//
// Das Panel kennt weder Player noch GPU: Es ruft einen Handler auf, der den Wert übernimmt und den
// tatsächlich gespeicherten (z. B. begrenzten) zurückgibt, damit die Anzeige stimmt.
#pragma once

#include <QGroupBox>
#include <QPointer>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <unordered_set>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class QComboBox;
class QLabel;
class QLineEdit;
class QScrollArea;
class QToolButton;

class ParameterPanel : public QWidget {
    Q_OBJECT

public:
    using ChangeHandler =
        std::function<std::optional<graph::ParameterValue>(NodeId, const std::string&, graph::ParameterValue)>;

    explicit ParameterPanel(QWidget* parent = nullptr);

    /// Skalierung am Kabel geändert: Verbindung, Faktor, Offset.
    using ScaleHandler = std::function<void(ConnectionId, double, double)>;

    void setChangeHandler(ChangeHandler handler) { handler_ = std::move(handler); }
    void setScaleHandler(ScaleHandler handler) { scaleHandler_ = std::move(handler); }
    /// Baut die Regler neu auf (nach dem Laden eines Projekts).
    void rebuild(const std::vector<runtime::NodeInfo>& nodes);

    /// Hebt die Gruppe des Nodes hervor und scrollt sie ins Bild. Ohne Wert (oder bei einem Node
    /// ohne Parameter) verschwindet die Hervorhebung. Wahr, wenn es für den Node eine Gruppe gibt.
    bool showNode(std::optional<NodeId> node);
    /// Der Node, dessen Gruppe gerade hervorgehoben ist.
    [[nodiscard]] std::optional<NodeId> shownNode() const noexcept { return shown_; }

    [[nodiscard]] QSize sizeHint() const override { return {340, 520}; }

    enum class Mode { Selected, All };
    void setMode(Mode mode);
    [[nodiscard]] Mode mode() const noexcept { return mode_; }
    /// Hängt die Gruppe eines Nodes an bzw. löst sie.
    void setPinned(NodeId node, bool pinned);
    [[nodiscard]] bool pinned(NodeId node) const { return pinned_.contains(node.value()); }
    /// Gruppe des Nodes sichtbar bzw. aufgeklappt (für den Rauchtest); false bei unbekanntem Node.
    [[nodiscard]] bool groupShown(NodeId node) const;
    [[nodiscard]] bool groupOpen(NodeId node) const;
    [[nodiscard]] std::size_t groupCount() const noexcept { return entries_.size(); }
    [[nodiscard]] QLineEdit* searchField() const noexcept { return search_; }

private:
    struct Entry {
        QPointer<QGroupBox> group;
        QPointer<QWidget> body;
        QPointer<QToolButton> arrow;
        QPointer<QToolButton> pin;
        QString text;  // Node- und Parameternamen in Kleinbuchstaben (fürs Suchfeld)
    };
    QWidget* makeScaleRow(const runtime::ParameterInfo& info, QWidget* parent);
    void applyVisibility();

    ChangeHandler handler_;
    ScaleHandler scaleHandler_;
    std::unordered_map<std::uint64_t, Entry> entries_;
    std::unordered_map<std::uint64_t, bool> open_;  // von Hand auf- oder zugeklappt (sonst gilt die Regel der Ansicht)
    std::unordered_set<std::uint64_t> pinned_;
    std::optional<NodeId> shown_;
    Mode mode_{Mode::Selected};
    QScrollArea* scroll_{nullptr};
    QComboBox* modeBox_{nullptr};
    QLineEdit* search_{nullptr};
    QPointer<QLabel> emptyHint_;
};

}  // namespace whitehole::studio
