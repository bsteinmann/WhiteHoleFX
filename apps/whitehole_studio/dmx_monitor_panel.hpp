// dmx_monitor_panel.hpp: reiner Lese-Monitor für ein DMX-Universe (Schritt 1 des DMX-Patch/Monitor-Gitters).
//
// Warum es das gibt: Bei der Fehlersuche ("kommt überhaupt etwas vom Pult an? auf welchem Kanal?") half
// bisher nur ein externes Tool. Dieses Panel zeigt alle 512 Kanäle eines wählbaren Protokoll+Universe als
// Gitter (16 Spalten, 32 Zeilen, Zeile n beginnt bei Kanal 16*n+1) mit dem zuletzt empfangenen Wert.
//
// Liest die Werte direkt aus input::DmxStore (dessen readAll), nicht über den Player, weil der Monitor
// unabhängig davon ist, welche Nodes im Projekt den Kanal überhaupt benutzen - genau das soll er ja zeigen,
// auch wenn (noch) nichts im Projekt darauf hört. Für "wer hört zu" (Tooltip) und den Rechtsklick (siehe
// unten) braucht er trotzdem den Player, nur eben zusätzlich.
//
// "Wer hört zu"-Overlay (Schritt 4 des DMX-Patch/Monitor-Gitters, siehe CLAUDE.md): der Tooltip einer
// Zelle nennt den Node (oder Master-Helligkeit/Live-DMX), der sie liest, falls einer das tut. Wird nur bei
// einem Protokoll-/Universe-Wechsel neu berechnet (nicht jeden Frame) - ändert sich der Graph, während der
// Monitor offen ist, zeigt der Tooltip den alten Stand, bis neu gewählt wird; das ist bewusst einfach
// gehalten (ein Durchlauf über alle Nodes pro Frame wäre für ein reines Anzeige-Werkzeug unnötig teuer).
//
// Rechtsklick (Schritt 3): bei einer schon belegten Zelle eine deaktivierte Zeile "Gehört zu: ..." (aus
// demselben Tooltip-Text), sonst die Wahl, den Kanal als Master-Helligkeit, Live-Look-Auswahl oder
// Live-Überblendzeit zu verwenden (die einzigen Eingangs-"Patches" ohne eigenen Node, siehe app_settings.hpp).
#pragma once

#include <QPoint>
#include <QString>
#include <QWidget>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

#include "whitehole/input/dmx_store.hpp"
#include "whitehole/runtime/player.hpp"

class QComboBox;
class QLabel;
class QMenu;
class QSpinBox;
class QTableWidget;

namespace whitehole::studio {

class DmxMonitorPanel : public QWidget {
    Q_OBJECT

public:
    DmxMonitorPanel(std::shared_ptr<input::DmxStore> store, runtime::Player& player, QWidget* parent = nullptr);

    /// Gleicht das Gitter mit dem Speicher ab (nur geänderte Zellen). Der Aufrufer (MainWindow::tick) ruft
    /// das nur, solange das Dock sichtbar ist - 512 Zellen jeden Frame sind sonst verschwendet.
    void refresh();

    /// Rauchtest für die CI: Protokoll/Universe wählen, einen Wert schreiben, prüfen, dass er erscheint.
    QString exercise();

Q_SIGNALS:
    /// Rechtsklick, "Als Master-Helligkeit-Kanal verwenden" gewählt.
    void requestMasterDmx(input::DmxProtocol protocol, std::uint32_t universe, std::uint32_t channel);
    /// Rechtsklick, "Als Live-Look-Auswahl-Kanal verwenden" gewählt.
    void requestLiveDmxSelect(input::DmxProtocol protocol, std::uint32_t universe, std::uint32_t channel);
    /// Rechtsklick, "Als Live-Überblendzeit-Kanal verwenden" gewählt.
    void requestLiveDmxFade(input::DmxProtocol protocol, std::uint32_t universe, std::uint32_t channel);

private:
    void onSelectionChanged();  // Protokoll oder Universe gewechselt: Gitter muss neu gezeichnet werden
    void updateListenerTooltips();
    void showContextMenu(const QPoint& pos);
    [[nodiscard]] QMenu* buildContextMenu(std::uint32_t channel, input::DmxProtocol protocol, std::uint32_t universe);
    [[nodiscard]] static QString listenerTooltip(std::uint32_t channel, const std::optional<runtime::DmxInputListener>& listener);

    std::shared_ptr<input::DmxStore> store_;
    runtime::Player& player_;
    QComboBox* protocol_{nullptr};
    QSpinBox* universe_{nullptr};
    QLabel* status_{nullptr};
    QTableWidget* grid_{nullptr};
    std::array<std::uint8_t, input::kDmxChannels> lastValues_{};
    bool hasData_{false};  // zeigt das Gitter gerade Werte (statt "-") an?
};

}  // namespace whitehole::studio
