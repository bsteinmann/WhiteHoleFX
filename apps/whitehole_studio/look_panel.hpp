// look_panel.hpp: die Look-Liste (Dock "Looks") im Bearbeiten-Modus.
//
// Warum es das gibt: Ein fertiger Effekt - vom Generator über die Modifier bis zum Ausgang - soll sich merken
// und später per Knopfdruck abrufen lassen. Dieses Panel verwaltet die Liste: den aktuellen Effekt als Look
// speichern, einen Look überschreiben, zur Bearbeitung in den Editor laden, umbenennen, die Überblendzeit
// einstellen, ordnen und löschen. Gespeichert wird mit dem Projekt (Strg+S). Abgerufen wird im Live-Modus
// (siehe live_panel.hpp).
//
// Die Logik steckt im Player; das Panel ruft nur auf. Jede Aktion gibt es auch ohne Dialog (saveLookAs
// usw.), damit der Rauchtest sie bedienen kann.
#pragma once

#include <QString>
#include <QWidget>

#include "whitehole/runtime/player.hpp"

class QDoubleSpinBox;
class QListWidget;
class QPushButton;

namespace whitehole::studio {

class LookPanel : public QWidget {
    Q_OBJECT

public:
    explicit LookPanel(runtime::Player& player, QWidget* parent = nullptr);

    /// Füllt die Liste neu aus dem Player (nach Laden, Speichern, Löschen ...) und behält die Auswahl.
    void rebuild();
    /// Der gewählte Look oder -1.
    [[nodiscard]] int selectedIndex() const;
    void select(int index);
    [[nodiscard]] int lookCount() const;

    // Aktionen ohne Dialog; Fehler gehen über `failed`. Wahr bei Erfolg.
    bool saveLookAs(const QString& name);
    bool overwriteLook(int index);
    bool loadIntoEditor(int index);
    bool renameLook(int index, const QString& name);
    bool removeLook(int index);
    bool moveSelected(int delta);

Q_SIGNALS:
    /// Die Liste oder das Projekt hat sich geändert (Titel und Menüs auffrischen).
    void changed();
    /// Der Editor-Graph wurde durch einen Look ersetzt (Editor und Parameter neu aufbauen).
    void loadedIntoEditor();
    /// Der Knopf "Live-Modus starten" wurde gedrückt; `first` ist der gewählte Look (oder 0).
    void liveRequested(int first);
    void failed(const QString& message);

private:
    void askSaveNew();
    void askOverwrite();
    void askRename();
    void askRemove();
    void onSelectionChanged();
    void updateButtons();
    [[nodiscard]] QString suggestedName() const;

    runtime::Player& player_;
    QListWidget* list_{nullptr};
    QDoubleSpinBox* fade_{nullptr};
    QPushButton* saveButton_{nullptr};
    QPushButton* overwriteButton_{nullptr};
    QPushButton* editButton_{nullptr};
    QPushButton* renameButton_{nullptr};
    QPushButton* removeButton_{nullptr};
    QPushButton* upButton_{nullptr};
    QPushButton* downButton_{nullptr};
    QPushButton* liveButton_{nullptr};
    bool updating_{false};
};

}  // namespace whitehole::studio
