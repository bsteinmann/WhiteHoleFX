// dmx_output_monitor_panel.hpp: reiner Lese-Monitor für ein gesendetes DMX-Universe (Schritt 2 des
// DMX-Patch/Monitor-Gitters, siehe CLAUDE.md).
//
// Warum es das gibt: Gegenstück zu dmx_monitor_panel.hpp, nur für die Ausgabe statt den Eingang - "kommt
// am Scheinwerfer wirklich das an, was ich erwarte?" ließ sich bisher nur mit einem externen DMX-Monitor
// zwischen Rechner und Controller prüfen. Zeigt alle 512 Kanäle eines wählbaren Protokoll+Universe als
// Gitter mit dem zuletzt gesendeten Wert (nicht dem zuletzt empfangenen - Eingang bleibt dmx_monitor_panel).
//
// Bewusst kein DDP: DDP adressiert Byte-Offsets eines Geräts, kein Universe-Konzept, passt nicht ins
// 512-Kanal-Gitter (siehe output::SentDmxStore). Bewusst nur Anzeige, keine Zuordnung per Rechtsklick -
// das ist Schritt 3.
#pragma once

#include <QWidget>

#include <array>
#include <cstdint>
#include <memory>

#include "whitehole/output/sent_dmx_store.hpp"

class QComboBox;
class QLabel;
class QSpinBox;
class QTableWidget;

namespace whitehole::studio {

class DmxOutputMonitorPanel : public QWidget {
    Q_OBJECT

public:
    explicit DmxOutputMonitorPanel(std::shared_ptr<output::SentDmxStore> store, QWidget* parent = nullptr);

    /// Gleicht das Gitter mit dem Speicher ab (nur geänderte Zellen). Der Aufrufer (MainWindow::tick) ruft
    /// das nur, solange das Dock sichtbar ist - 512 Zellen jeden Frame sind sonst verschwendet.
    void refresh();

    /// Rauchtest für die CI: Protokoll/Universe wählen, einen Wert schreiben, prüfen, dass er erscheint.
    QString exercise();

private:
    void onSelectionChanged();  // Protokoll oder Universe gewechselt: Gitter muss neu gezeichnet werden

    std::shared_ptr<output::SentDmxStore> store_;
    QComboBox* protocol_{nullptr};
    QSpinBox* universe_{nullptr};
    QLabel* status_{nullptr};
    QTableWidget* grid_{nullptr};
    std::array<std::uint8_t, output::kSentDmxChannels> lastValues_{};
    bool hasData_{false};  // zeigt das Gitter gerade Werte (statt "-") an?
};

}  // namespace whitehole::studio
