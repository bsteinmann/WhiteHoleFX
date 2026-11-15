// dmx_monitor_panel.hpp: reiner Lese-Monitor für ein DMX-Universe (Schritt 1 des DMX-Patch/Monitor-Gitters).
//
// Warum es das gibt: Bei der Fehlersuche ("kommt überhaupt etwas vom Pult an? auf welchem Kanal?") half
// bisher nur ein externes Tool. Dieses Panel zeigt alle 512 Kanäle eines wählbaren Protokoll+Universe als
// Gitter (16 Spalten, 32 Zeilen, Zeile n beginnt bei Kanal 16*n+1) mit dem zuletzt empfangenen Wert.
//
// Bewusst nur Anzeige: kein Rechtsklick, keine Zuordnung zu Fixture-Patch oder Kanal-Pool (das kommt erst
// in einem späteren Schritt, siehe CLAUDE.md). Liest direkt aus input::DmxStore (dessen readAll), nicht
// über den Player, weil der Monitor unabhängig davon ist, welche Nodes im Projekt den Kanal überhaupt
// benutzen - genau das soll er ja zeigen, auch wenn (noch) nichts im Projekt darauf hört.
#pragma once

#include <QWidget>

#include <array>
#include <cstdint>
#include <memory>

#include "whitehole/input/dmx_store.hpp"

class QComboBox;
class QLabel;
class QSpinBox;
class QTableWidget;

namespace whitehole::studio {

class DmxMonitorPanel : public QWidget {
    Q_OBJECT

public:
    explicit DmxMonitorPanel(std::shared_ptr<input::DmxStore> store, QWidget* parent = nullptr);

    /// Gleicht das Gitter mit dem Speicher ab (nur geänderte Zellen). Der Aufrufer (MainWindow::tick) ruft
    /// das nur, solange das Dock sichtbar ist - 512 Zellen jeden Frame sind sonst verschwendet.
    void refresh();

    /// Rauchtest für die CI: Protokoll/Universe wählen, einen Wert schreiben, prüfen, dass er erscheint.
    QString exercise();

private:
    void onSelectionChanged();  // Protokoll oder Universe gewechselt: Gitter muss neu gezeichnet werden

    std::shared_ptr<input::DmxStore> store_;
    QComboBox* protocol_{nullptr};
    QSpinBox* universe_{nullptr};
    QLabel* status_{nullptr};
    QTableWidget* grid_{nullptr};
    std::array<std::uint8_t, input::kDmxChannels> lastValues_{};
    bool hasData_{false};  // zeigt das Gitter gerade Werte (statt "-") an?
};

}  // namespace whitehole::studio
