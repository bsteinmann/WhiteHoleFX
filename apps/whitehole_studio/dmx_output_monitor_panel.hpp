// dmx_output_monitor_panel.hpp: reiner Lese-Monitor für ein gesendetes DMX-Universe (Schritt 2 des
// DMX-Patch/Monitor-Gitters, siehe CLAUDE.md).
//
// Warum es das gibt: Gegenstück zu dmx_monitor_panel.hpp, nur für die Ausgabe statt den Eingang - "kommt
// am Scheinwerfer wirklich das an, was ich erwarte?" ließ sich bisher nur mit einem externen DMX-Monitor
// zwischen Rechner und Controller prüfen. Zeigt alle 512 Kanäle eines wählbaren Protokoll+Universe als
// Gitter mit dem zuletzt gesendeten Wert (nicht dem zuletzt empfangenen - Eingang bleibt dmx_monitor_panel).
//
// Bewusst kein DDP: DDP adressiert Byte-Offsets eines Geräts, kein Universe-Konzept, passt nicht ins
// 512-Kanal-Gitter (siehe output::SentDmxStore).
//
// Tooltip (Schritt 4, "Wer hört zu"-Overlay, nachträglich auch für die Ausgabe freigegeben): jede Zelle
// zeigt per Player::dmxOutputOccupantAt, was auf diesen Kanal schon gepatcht ist (Fixture oder DMX-Kanal-
// Pool) - analog zum Eingangsmonitor (dmx_monitor_panel.hpp), nur berechnet bei Protokoll-/Universe-
// Wechsel, nicht in refresh() (siehe dort für die Begründung).
//
// Rechtsklick (Schritt 3 des DMX-Patch/Monitor-Gitters): auf einer freien Zelle bietet er an, dort ein
// neues Fixture zu patchen (öffnet den bestehenden "Fixture hinzufügen"-Dialog vorbefüllt, statt das
// Patchen hier noch einmal nachzubauen). Auf einer belegten Zelle (nachträglich freigegebene Erweiterung)
// bietet er zusätzlich "Farbe ändern…" (siehe unten) an und springt sonst direkt zur passenden Zeile in der
// Fixture-Patch- bzw. DMX-Kanal-Pool-Tabelle auf der Seite Ausgabe
// (StageSetupWindow::openOutputFixture/openOutputPool), statt nur anzuzeigen, wer belegt.
//
// Zellenfarben (nachträglich freigegebene Erweiterung, erster Teil des 15.11.-Vorschlags "farbig
// hinterlegen"): eine belegte Zelle bekommt automatisch die Standardfarbe ihrer Kategorie
// (dmx_cell_colors.hpp: Fixture oder Pool), eine freie bleibt neutral; wie der Tooltip nur bei einem
// Protokoll-/Universe-Wechsel neu berechnet (siehe updateOccupantTooltips), nicht jeden Frame. Per
// Rechtsklick → "Farbe ändern…" lässt sich die Farbe einer schon belegten Zelle einzeln überschreiben; die
// Überschreibung gilt nur für die aktuell gewählte Protokoll-/Universe-Kombination dieses Panels und geht
// beim Wechsel wieder verloren (keine Projekt- oder Einstellungs-Daten, rein optisch), genauso wie im
// Eingangs-Monitor (dmx_monitor_panel.hpp).
#pragma once

#include <QColor>
#include <QPoint>
#include <QWidget>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>

#include "whitehole/mapping/layout.hpp"
#include "whitehole/output/sent_dmx_store.hpp"
#include "whitehole/runtime/player.hpp"

class QComboBox;
class QLabel;
class QMenu;
class QSpinBox;
class QTableWidget;

namespace whitehole::studio {

class DmxOutputMonitorPanel : public QWidget {
    Q_OBJECT

public:
    DmxOutputMonitorPanel(std::shared_ptr<output::SentDmxStore> store, runtime::Player& player, QWidget* parent = nullptr);

    /// Gleicht das Gitter mit dem Speicher ab (nur geänderte Zellen). Der Aufrufer (MainWindow::tick) ruft
    /// das nur, solange das Dock sichtbar ist - 512 Zellen jeden Frame sind sonst verschwendet.
    void refresh();

    /// Rauchtest für die CI: Protokoll/Universe wählen, einen Wert schreiben, prüfen, dass er erscheint,
    /// Tooltip ohne Belegung prüfen, Rechtsklick auf eine freie, eine Fixture- und eine Pool-Zelle prüfen,
    /// dazu deren Zellenfarben und dass "Farbe ändern…" bei belegten Zellen angeboten wird.
    QString exercise();

Q_SIGNALS:
    /// Rechtsklick auf eine freie Zelle, "Neues Fixture hier patchen…" gewählt.
    void requestManualPatch(output::Protocol protocol, std::uint32_t universe, std::uint32_t channel);
    /// Rechtsklick auf eine von einem Fixture belegte Zelle, "Fixture-Patch bearbeiten" gewählt.
    void requestEditFixturePatch(mapping::FixtureId id);
    /// Rechtsklick auf eine vom DMX-Kanal-Pool belegte Zelle, "DMX-Kanal-Pool-Patch bearbeiten" gewählt.
    void requestEditPoolPatch(std::uint16_t poolChannel);

private:
    void onSelectionChanged();  // Protokoll oder Universe gewechselt: Gitter muss neu gezeichnet werden
    void showContextMenu(const QPoint& pos);
    /// Baut das Menü für einen Kanal, ohne es anzuzeigen - exec() würde im Rauchtest blockieren (niemand
    /// klickt), darum ruft exercise() diese Funktion statt showContextMenu direkt auf. Aufrufer übernimmt
    /// den Besitz (Qt::WA_DeleteOnClose reicht hier nicht, da nie gezeigt).
    [[nodiscard]] QMenu* buildContextMenu(std::uint32_t channel, output::Protocol protocol, std::uint32_t universe);
    /// Berechnet die Tooltips aller 512 Zellen neu (siehe Kommentar am Dateianfang: nur hier, nicht in refresh()).
    void updateOccupantTooltips();
    [[nodiscard]] static QString occupantTooltip(std::uint32_t channel, const std::optional<runtime::DmxOutputOccupant>& occupant);
    /// Rechtsklick → "Farbe ändern…" auf einer belegten Zelle: siehe dmx_monitor_panel.hpp für die
    /// Begründung, warum exercise() diese Aktion nur auf Vorhandensein prüft, statt sie auszulösen.
    void changeCellColor(std::uint32_t channel);

    std::shared_ptr<output::SentDmxStore> store_;
    runtime::Player& player_;
    QComboBox* protocol_{nullptr};
    QSpinBox* universe_{nullptr};
    QLabel* status_{nullptr};
    QTableWidget* grid_{nullptr};
    std::array<std::uint8_t, output::kSentDmxChannels> lastValues_{};
    bool hasData_{false};  // zeigt das Gitter gerade Werte (statt "-") an?
    std::map<std::uint32_t, QColor> colorOverrides_;  // Kanal -> von Hand gewählte Farbe, siehe Kommentar oben
};

}  // namespace whitehole::studio
