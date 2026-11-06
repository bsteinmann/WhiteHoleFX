// port_legend.hpp: schwebendes Fenster "Legende" mit den Farben und Formen der Node-Ports.
//
// Warum es das gibt: Zwölf Porttypen unterscheiden sich nur durch Farbe und Form; wer neu ist, weiß nicht, was ein
// roter Rhombus oder ein blaues Dreieck bedeutet. Das Fenster bleibt offen neben dem Node-Editor (verschiebbar) und
// zeichnet die Marker mit derselben Funktion wie der Editor, kann also nie von ihm abweichen.
#pragma once

#include <QWidget>

namespace whitehole::studio {

class PortLegend : public QWidget {
    Q_OBJECT

public:
    explicit PortLegend(QWidget* parent = nullptr);
    /// Anzahl der Zeilen mit Porttypen (für den Rauchtest der CI).
    [[nodiscard]] int typeRows() const noexcept { return typeRows_; }

private:
    int typeRows_{0};
};

}  // namespace whitehole::studio
