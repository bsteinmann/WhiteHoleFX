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
// Das Panel kennt weder Player noch GPU: Es ruft einen Handler auf, der den Wert übernimmt und den
// tatsächlich gespeicherten (z. B. begrenzten) zurückgibt, damit die Anzeige stimmt.
#pragma once

#include <QGroupBox>
#include <QPointer>
#include <QScrollArea>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class ParameterPanel : public QScrollArea {
    Q_OBJECT

public:
    using ChangeHandler =
        std::function<std::optional<graph::ParameterValue>(NodeId, const std::string&, graph::ParameterValue)>;

    explicit ParameterPanel(QWidget* parent = nullptr);

    void setChangeHandler(ChangeHandler handler) { handler_ = std::move(handler); }
    /// Baut die Regler neu auf (nach dem Laden eines Projekts).
    void rebuild(const std::vector<runtime::NodeInfo>& nodes);

    /// Hebt die Gruppe des Nodes hervor und scrollt sie ins Bild. Ohne Wert (oder bei einem Node
    /// ohne Parameter) verschwindet die Hervorhebung. Wahr, wenn es für den Node eine Gruppe gibt.
    bool showNode(std::optional<NodeId> node);
    /// Der Node, dessen Gruppe gerade hervorgehoben ist.
    [[nodiscard]] std::optional<NodeId> shownNode() const noexcept { return shown_; }

    [[nodiscard]] QSize sizeHint() const override { return {340, 520}; }

private:
    ChangeHandler handler_;
    std::unordered_map<std::uint64_t, QPointer<QGroupBox>> groups_;
    std::optional<NodeId> shown_;
};

}  // namespace whitehole::studio
