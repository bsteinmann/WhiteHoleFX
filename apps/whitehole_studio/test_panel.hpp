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

#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace whitehole::studio {

class LayoutPanel;

class TestPanel : public QWidget {
    Q_OBJECT

public:
    TestPanel(runtime::Player& player, LayoutPanel& layoutPanel, QWidget* parent = nullptr);

    /// Stellt die Bedienelemente auf "Aus" (nach dem Laden eines Projekts; der Player hat das
    /// Testbild dann schon verworfen).
    void reset();
    /// Das aktive Testbild als Text für Banner und Statuszeile, leer ohne Testbild.
    [[nodiscard]] QString description() const;

    /// Rauchtest für die CI: jede Art einschalten, prüfen, dass der Player sie kennt, und wieder
    /// ausschalten. Leer bei Erfolg.
    QString exercise();

    [[nodiscard]] QSize sizeHint() const override { return {360, 360}; }

Q_SIGNALS:
    /// Ein Testbild wurde ein-, aus- oder umgeschaltet.
    void patternChanged();
    void failed(const QString& message);

private:
    /// Liest die Bedienelemente und setzt das Testbild beim Player.
    void apply();
    /// Die Art wurde gewechselt: Tempo auf den Standard der Art, Beschriftung anpassen.
    void kindChanged();
    void updateEnabled();
    [[nodiscard]] runtime::TestPatternKind currentKind() const;

    runtime::Player& player_;
    LayoutPanel& layout_;
    QComboBox* kind_{nullptr};
    QComboBox* color_{nullptr};
    QComboBox* axis_{nullptr};
    QDoubleSpinBox* rate_{nullptr};
    QCheckBox* onlySelected_{nullptr};
    QPushButton* stop_{nullptr};
    QLabel* hint_{nullptr};
    bool updating_{false};  // Bedienelemente werden gerade vom Programm gesetzt
};

}  // namespace whitehole::studio
