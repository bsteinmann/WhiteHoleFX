// test_panel.hpp: Bedienfeld für Testbilder (Farbe, Auswahl, Laufbalken, Farbreihenfolge, Lauflicht).
//
// Warum es diese Datei gibt: Beim Aufbau einer Wand will man jede LED und jede Farbreihenfolge
// prüfen, ohne dafür den Effekt umzubauen. Das Panel wählt ein Testbild und gibt es an den Player;
// der ersetzt damit die Farben des Effekts (Vorschau und Ausgabe). Das Testbild gehört nicht zum
// Projekt. Gesendet wird es nur, wenn "Ausgabe senden" an ist. Solange ein Testbild aktiv ist,
// zeigt das Hauptfenster in der Vorschau ein rotes Banner "TESTMODUS".
#pragma once

#include <QString>
#include <QWidget>

#include <vector>

#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QPushButton;

namespace whitehole::studio {

class TestPanel : public QWidget {
    Q_OBJECT

public:
    explicit TestPanel(runtime::Player& player, QWidget* parent = nullptr);

    /// Stellt die Bedienelemente auf "Aus" (nach dem Laden eines Projekts; der Player hat das
    /// Testbild dann schon verworfen).
    void reset();
    /// Das aktive Testbild als Text für Banner und Statuszeile, leer ohne Testbild.
    [[nodiscard]] QString description() const;

    /// Liest die Fixtures des Projekts neu in die Liste (nach Laden, Anlegen, Entfernen). Die Auswahl bleibt, wo es sie noch gibt.
    void rebuild();
    /// Die im Testfenster gewählten Fixtures (Liste und Klick in die Ansicht), in Projektreihenfolge.
    [[nodiscard]] std::vector<mapping::FixtureId> selectedIds() const;
    /// Ein Klick auf eine LED in der Test-Ansicht: wählt ihr Fixture (mit `extend` dazu bzw. weg, wie Strg+Klick).
    void pickFixture(mapping::FixtureId id, bool extend);

    /// Rauchtest für die CI: jede Art einschalten, prüfen, dass der Player sie kennt, und wieder
    /// ausschalten. Leer bei Erfolg.
    QString exercise();

    [[nodiscard]] QSize sizeHint() const override { return {360, 360}; }

Q_SIGNALS:
    /// Ein Testbild wurde ein-, aus- oder umgeschaltet.
    void patternChanged();
    /// Die Auswahl der Fixtures im Testfenster hat sich geändert (Liste oder Klick in die Ansicht).
    void selectionChanged();
    void failed(const QString& message);

private:
    /// Liest die Bedienelemente und setzt das Testbild beim Player.
    void apply();
    /// Die Art wurde gewechselt: Tempo auf den Standard der Art, Beschriftung anpassen.
    void kindChanged();
    void updateEnabled();
    [[nodiscard]] runtime::TestPatternKind currentKind() const;

    runtime::Player& player_;
    QComboBox* kind_{nullptr};
    QComboBox* color_{nullptr};
    QComboBox* axis_{nullptr};
    QDoubleSpinBox* rate_{nullptr};
    QCheckBox* onlySelected_{nullptr};
    QListWidget* fixtures_{nullptr};  // eigene Auswahl fürs Testen, unabhängig von der Layout-Liste
    QPushButton* stop_{nullptr};
    QLabel* hint_{nullptr};
    bool updating_{false};  // Bedienelemente werden gerade vom Programm gesetzt
};

}  // namespace whitehole::studio
