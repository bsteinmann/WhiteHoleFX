// output_form.hpp: Ziele (Adressen der Controller) und Projekteinstellungen.
//
// Ziele sind die Zeilen einer Tabelle (Adresse, Port; Spalten verschiebbar); ihre Nummer steht links
// und wird in den Patches der Fixtures benutzt. Jedes Ziel lässt sich entfernen. Eine leere Adresse heißt Multicast (nur sACN). Port 0 heißt
// Standardport des Protokolls. Darunter stehen Bildrate, Normalisierung, sACN-Priorität und
// Quellname. Wie beim Fixture-Formular prüft der Player jede Änderung; bei einer Ablehnung meldet
// `failed` den Grund und die Anzeige springt zurück.
#pragma once

#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QString>
#include <QTableWidget>
#include <QWidget>

#include "form_helpers.hpp"
#include "whitehole/runtime/player.hpp"

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

private:
    void applyDestinations();
    void applySettings();
    void report(const Result<void>& result);

    runtime::Player& player_;
    QTableWidget* table_{nullptr};
    QDoubleSpinBox* fps_{nullptr};
    QComboBox* normalize_{nullptr};
    QSpinBox* priority_{nullptr};
    QLineEdit* sourceName_{nullptr};
    bool populating_{false};
};

}  // namespace whitehole::studio
