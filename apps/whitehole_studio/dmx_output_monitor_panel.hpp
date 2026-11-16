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
// Pool) - analog zum Eingangsmonitor (dmx_monitor_panel.hpp), aus refresh() heraus bei jedem Bild neu
// berechnet, solange der Reiter sichtbar ist (vom Nutzer gemeldet: vorher nur bei Protokoll-/Universe-
// Wechsel, dadurch zeigte der Monitor nach einem Patch/Aufheben anderswo die alte Farbe/den alten Tooltip).
//
// Rechtsklick (Schritt 3 des DMX-Patch/Monitor-Gitters): auf einer freien Zelle bietet er ein Untermenü
// "Node" (Gegenstück zum gleichnamigen Untermenü im Eingangs-Monitor, siehe dmx_monitor_panel.hpp): listet
// alle wh.output.dmx/wh.output.dmxcolor-Nodes des Projekts (Player::dmxOutputValueNodes) und patcht den
// gewählten Node-Pool-Bereich auf diesen Kanal (requestPatchOutputNode, siehe
// MainWindow::patchDmxOutputValueNode) - bewusst getrennt vom Eingangs-Monitor-Untermenü (nicht "egal
// welcher Monitor", wie zunächst erwogen): Empfangs- und Sende-Adressraum sind unterschiedliche Dinge, auch
// bei gleicher Universe-Nummer, und Fixtures bleiben bewusst außen vor - sie werden ausschließlich über die
// Fixture-Patch-Tabelle selbst gepatcht, nie über dieses Untermenü. ("Neues Fixture hier patchen…" gab es
// hier früher zusätzlich flach auf der freien Zelle, vom Nutzer wieder entfernt - Fixtures patcht man über
// den Reiter Fixtures, nicht hier.) Auf einer belegten Zelle (nachträglich freigegebene Erweiterung) bietet
// er zusätzlich "Farbe ändern…" (siehe unten) an; bei einem Fixture springt er direkt zur passenden Zeile
// der Fixture-Patch-Tabelle (StageSetupWindow::openOutputFixture); bei einem DMX-Kanal-Pool-Patch (also
// einem Node) bietet er stattdessen, wie die freie Zelle, ein Untermenü "Node" mit "Patch bearbeiten…"
// (springt zur DMX-Kanal-Pool-Tabelle, StageSetupWindow::openOutputPool) und, neu, "Patch aufheben" (entfernt
// den Patch direkt, ohne den Umweg über die Tabelle - vom Nutzer gewünscht: "rechtklick -> node -> patch
// aufheben").
//
// Zellenfarben (nachträglich freigegebene Erweiterung, erster Teil des 15.11.-Vorschlags "farbig
// hinterlegen"): eine belegte Zelle bekommt automatisch die Standardfarbe ihrer Kategorie
// (dmx_cell_colors.hpp: Fixture oder Pool), eine freie bleibt neutral; wie der Tooltip bei jedem Bild neu
// berechnet, solange der Reiter sichtbar ist (siehe updateOccupantTooltips). Per
// Rechtsklick → "Farbe ändern…" lässt sich die Farbe einer schon belegten Zelle einzeln überschreiben; die
// Überschreibung gilt nur für die aktuell gewählte Protokoll-/Universe-Kombination dieses Panels und geht
// beim Wechsel wieder verloren (keine Projekt- oder Einstellungs-Daten, rein optisch), genauso wie im
// Eingangs-Monitor (dmx_monitor_panel.hpp).
//
// Zellentext: nur die reine Zahl (vom Nutzer so entschieden, siehe dmx_monitor_panel.hpp - eine Zeitlang
// stand hier zusätzlich der Name der Belegung in einer zweiten Zeile, das schnitt bei langen Namen ab und
// wurde zurückgenommen); wer die Zelle belegt, steht ausschließlich im Tooltip (occupantTooltip).
#pragma once

#include <QColor>
#include <QPoint>
#include <QString>
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
    /// dazu deren Zellenfarben, dass "Farbe ändern…" bei belegten Zellen angeboten wird, das Untermenü
    /// "Node" auf einer freien Zelle (Patchen eines wh.output.dmx-Nodes) und auf einer belegten Pool-Zelle
    /// ("Patch bearbeiten…" und "Patch aufheben").
    QString exercise();

Q_SIGNALS:
    /// Rechtsklick auf eine von einem Fixture belegte Zelle, "Fixture-Patch bearbeiten" gewählt.
    void requestEditFixturePatch(mapping::FixtureId id);
    /// Rechtsklick auf eine vom DMX-Kanal-Pool belegte Zelle, Untermenü "Node", "Patch bearbeiten…" gewählt.
    void requestEditPoolPatch(std::uint16_t poolChannel);
    /// Rechtsklick auf eine vom DMX-Kanal-Pool belegte Zelle, Untermenü "Node", "Patch aufheben" gewählt:
    /// den Patch auf diesem Pool-Kanal direkt entfernen (siehe MainWindow::buildDmxMonitor).
    void requestRemovePoolPatch(std::uint16_t poolChannel);
    /// Rechtsklick auf eine freie Zelle, Untermenü "Node", einen wh.output.dmx/wh.output.dmxcolor-Node
    /// gewählt: dessen Pool-Bereich (poolChannel/count, siehe DmxOutputValueNodeRef) auf diese Adresse patchen.
    void requestPatchOutputNode(std::uint16_t poolChannel, std::uint16_t count, output::Protocol protocol,
                                 std::uint32_t universe, std::uint32_t channel);

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
