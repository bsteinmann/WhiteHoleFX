// output_form.hpp: Ziele (Adressen der Controller) und Projekteinstellungen.
//
// Controller sind die Zeilen einer Tabelle (Name, Protokoll, Adresse, Port; Spalten verschiebbar); ihre Nummer steht links
// und wird in den Patches der Fixtures benutzt. Das Protokoll (Art-Net, sACN, DDP) gehört zum Controller; wechselt es,
// ziehen die Patches mit (Player::setDestinations). Jeder Controller lässt sich entfernen. Eine leere Adresse heißt Multicast (nur sACN). Ein leerer Port heißt
// Standardport des Protokolls. Darunter stehen Bildrate, Normalisierung und (nur mit einem sACN-Controller) sACN-Priorität und
// Quellname. Wie beim Fixture-Formular prüft der Player jede Änderung; bei einer Ablehnung meldet
// `failed` den Grund und die Anzeige springt zurück.
#pragma once

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QString>
#include <QTableWidget>
#include <QWidget>

#include "form_helpers.hpp"
#include "whitehole/runtime/player.hpp"

class QMenu;

namespace whitehole::studio {

class OutputForm : public QWidget {
    Q_OBJECT

public:
    explicit OutputForm(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest alles neu aus dem Player (nach Laden, Undo, Redo).
    void rebuild();

    void addDestination();
    /// Entfernt das gewählte Ziel (beliebig, nicht nur das letzte). Ohne Auswahl das letzte. Fixtures auf höheren Zielen
    /// rücken mit; wird das Ziel noch benutzt, bleibt es und `failed` nennt das Fixture.
    void removeSelectedDestination();

    /// Rauchtest für die CI: Bildrate ändern, Ziele anlegen, ein mittleres und das letzte entfernen. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);
    /// Eine Meldung ohne Fehler (zum Beispiel: Universe wurden beim Protokollwechsel verschoben).
    void notice(const QString& message);

private:
    enum Column { Name, Protocol, Address, Port, Count };

    /// Bibliothek der Controller (output_form_library.cpp): in einem Ordner des Benutzers gespeichert, in jedem Projekt nutzbar.
    void saveSelectedToLibrary();
    void addFromLibrary(const output::Destination& controller);
    enum class LibraryAction { Add, Apply, Remove };
    void applyFromLibrary(const output::Destination& controller);
    void fillLibraryMenu(QMenu* menu, LibraryAction action);
    QString exerciseLibrary();
    void updateSacnVisibility();
    void applyDestinations();
    void applySettings();
    void report(const Result<void>& result);

    runtime::Player& player_;
    QTableWidget* table_{nullptr};
    QDoubleSpinBox* fps_{nullptr};
    QComboBox* normalize_{nullptr};
    QSpinBox* priority_{nullptr};
    QLineEdit* sourceName_{nullptr};
    QFormLayout* sacnLayout_{nullptr};
    QMenu* addMenu_{nullptr};
    QMenu* applyMenu_{nullptr};
    QMenu* deleteMenu_{nullptr};
    bool populating_{false};
};

}  // namespace whitehole::studio
