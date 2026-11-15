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
// Rechtsklick (Schritt 3 des DMX-Patch/Monitor-Gitters): zeigt, was auf diesen Kanal schon gepatcht ist
// (Fixture oder DMX-Kanal-Pool, über Player::dmxOutputOccupantAt), oder bietet bei einer freien Zelle an,
// dort ein neues Fixture zu patchen (öffnet den bestehenden "Fixture hinzufügen"-Dialog vorbefüllt, statt
// das Patchen hier noch einmal nachzubauen).
#pragma once

#include <QPoint>
#include <QWidget>

#include <array>
#include <cstdint>
#include <memory>

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
    /// Rechtsklick auf eine freie Zelle prüfen.
    QString exercise();

Q_SIGNALS:
    /// Rechtsklick auf eine freie Zelle, "Neues Fixture hier patchen…" gewählt.
    void requestManualPatch(output::Protocol protocol, std::uint32_t universe, std::uint32_t channel);

private:
    void onSelectionChanged();  // Protokoll oder Universe gewechselt: Gitter muss neu gezeichnet werden
    void showContextMenu(const QPoint& pos);
    /// Baut das Menü für einen Kanal, ohne es anzuzeigen - exec() würde im Rauchtest blockieren (niemand
    /// klickt), darum ruft exercise() diese Funktion statt showContextMenu direkt auf. Aufrufer übernimmt
    /// den Besitz (Qt::WA_DeleteOnClose reicht hier nicht, da nie gezeigt).
    [[nodiscard]] QMenu* buildContextMenu(std::uint32_t channel, output::Protocol protocol, std::uint32_t universe);

    std::shared_ptr<output::SentDmxStore> store_;
    runtime::Player& player_;
    QComboBox* protocol_{nullptr};
    QSpinBox* universe_{nullptr};
    QLabel* status_{nullptr};
    QTableWidget* grid_{nullptr};
    std::array<std::uint8_t, output::kSentDmxChannels> lastValues_{};
    bool hasData_{false};  // zeigt das Gitter gerade Werte (statt "-") an?
};

}  // namespace whitehole::studio
