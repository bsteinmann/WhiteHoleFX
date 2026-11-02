// cue_panel.hpp: die Cuelist im Fenster (Issue #21): Tabelle der Cues, Start/Stopp, GO.
//
// Warum es das gibt: Looks einzeln abzurufen genügt für Improvisation, eine Show braucht eine Reihenfolge mit
// Auslösern (GO, Zeit, Taste, DMX, Beat) und Zeiten (Delay, Fade). Die Liste gehört zum Projekt; dieses Panel zeigt und
// bearbeitet sie und zeigt im Betrieb, welcher Cue läuft (grün), welcher als Nächstes dran ist (orange) und welcher
// noch im Delay wartet (gelb). Der Ablauf selbst steckt im Player (cuesStart, cueGo, ...).
//
// Start und Stopp erledigt das Hauptfenster (es muss dafür in den Live-Modus wechseln); das Panel meldet nur den
// Wunsch. Tasten-Cues bekommen beim Start je ein Tastenkürzel, das beim Stopp wieder verschwindet.
#pragma once

#include <QWidget>

#include <vector>

#include "whitehole/runtime/player.hpp"

class QPushButton;
class QShortcut;
class QTableWidget;

namespace whitehole::studio {

class CuePanel : public QWidget {
    Q_OBJECT

public:
    explicit CuePanel(runtime::Player& player, QWidget* parent = nullptr);

    /// Baut die Tabelle aus der Liste im Player neu auf (nach Laden oder Ändern).
    void rebuild();
    /// Färbt die Zeilen nach dem Zustand des Ablaufs und gleicht die Tastenkürzel ab (jeden Frame; billig).
    void refresh();

    [[nodiscard]] int cueCount() const;
    /// Hängt einen Cue an (Look: der gewählte oder der erste). Gibt es keine Looks, wird es ein Schwarz-Cue.
    bool addCue();
    bool removeSelected();
    bool moveSelected(int delta);
    /// Ersetzt den Cue in einer Zeile (der Dialog ruft das auf, der Rauchtest auch).
    bool applyCue(int row, const project::Cue& cue);
    bool editRow(int row);
    [[nodiscard]] int selectedRow() const;

Q_SIGNALS:
    void failed(const QString& message);
    void changed();         // die Liste wurde verändert
    void startRequested();  // "Start": das Hauptfenster geht in den Live-Modus und startet den Ablauf
    void stopRequested();

private:
    bool commit(std::vector<project::Cue> cues, int selectRow);
    void syncShortcuts();
    void updateButtons();

    runtime::Player& player_;
    QTableWidget* table_{nullptr};
    QPushButton* startButton_{nullptr};
    QPushButton* goButton_{nullptr};
    QPushButton* addButton_{nullptr};
    QPushButton* removeButton_{nullptr};
    QPushButton* editButton_{nullptr};
    QPushButton* upButton_{nullptr};
    QPushButton* downButton_{nullptr};
    std::vector<QShortcut*> shortcuts_;
    bool shortcutsActive_{false};
    int lastCurrent_{-2};
    int lastNext_{-2};
    int lastPending_{-2};
};

}  // namespace whitehole::studio
