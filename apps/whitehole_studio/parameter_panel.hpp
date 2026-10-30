// parameter_panel.hpp: Regler für alle Parameter eines Projekts.
//
// Für jeden Node mit Parametern eine Gruppe, darin je Parameter eine Zeile mit Namen, passendem
// Bedienelement und einem Knopf zum Zurücksetzen auf den Standardwert:
//   Zahl mit Bereich   Schieberegler und Zahlenfeld
//   Zahl ohne Bereich  Zahlenfeld
//   Ganzzahl           Zahlenfeld
//   Ja/Nein            Kästchen
//   2D-/3D-Wert        je Komponente ein Zahlenfeld
//   Farbe              Farbfläche, ein Klick öffnet die Farbwahl (Eingabe sRGB, gespeichert linear)
//   Text               Eingabefeld
// Parameter, die eine Verbindung im Graphen steuert, sind ausgegraut.
//
// Das Panel kennt weder Player noch GPU: Es ruft einen Handler auf, der den Wert übernimmt und den
// tatsächlich gespeicherten (z. B. begrenzten) zurückgibt, damit die Anzeige stimmt.
#pragma once

#include <QScrollArea>

#include <functional>
#include <optional>
#include <string>
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

    [[nodiscard]] QSize sizeHint() const override { return {340, 520}; }

private:
    ChangeHandler handler_;
};

}  // namespace whitehole::studio
