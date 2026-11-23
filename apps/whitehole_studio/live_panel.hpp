// live_panel.hpp: die Bedienfläche für den Live-Betrieb.
//
// Warum es das gibt: Auf der Bühne muss ein Look mit einem Klick oder einer Taste abrufbar sein, ohne
// Editor, Parameter und Menüs. Dieses Panel zeigt für jeden gespeicherten Look einen großen Knopf (Tasten
// 1-9 für die ersten neun), dazu GO, die Überblendzeit und einen Balken für den laufenden Fade.
//
// Zwei Arten zu schalten:
//   - Direkt (Standard): ein Klick blendet sofort zum Look über - ein Handgriff, kaum Fehlbedienung.
//   - Flash (je Look, Rechtsklick): der Look läuft nur, solange der Knopf gedrückt ist; Vorrang vor "Mit GO".
//   - Mit GO: ein Klick lädt den Look nur vor ("Next", orange); GO (Leertaste) startet dann die Überblendung.
//     So kann der Shader vorher bereitgestellt werden, und der Fade selbst läuft ohne Ruckler.
// Farben: grün = läuft, orange = vorgeladen, gelb = blendet gerade ein.
//
// Die Logik steckt im Player (liveTrigger/liveArm/liveGo); das Panel ruft nur auf und zeigt liveStatus().
#pragma once

#include <QWidget>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "live_learn.hpp"
#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QProgressBar;
class QShortcut;
class QMenu;
class QPushButton;

namespace whitehole::studio {

class LivePanel : public QWidget {
    Q_OBJECT

public:
    explicit LivePanel(runtime::Player& player, QWidget* parent = nullptr);

    /// Baut die Look-Knöpfe aus der Liste im Player neu auf (nach Änderungen der Liste).
    void rebuild();
    /// Gleicht Farben, Name und Balken mit dem Zustand des Players ab (jeden Frame; billig).
    void refresh();

    /// Ein Look wird abgerufen: direkt überblenden oder (Modus "Mit GO") vorladen.
    bool trigger(int index);
    /// Startet die Überblendung zum vorgeladenen Look.
    bool go();
    [[nodiscard]] int buttonCount() const { return static_cast<int>(buttons_.size()); }
    void setUseGo(bool useGo);
    [[nodiscard]] bool useGo() const;
    /// Eigene Überblendzeit statt der des Looks; leer = Zeit des Looks.
    void setFadeOverride(std::optional<double> seconds);
    /// Die eingestellte eigene Überblendzeit; leer = Zeit des Looks.
    [[nodiscard]] std::optional<double> fadeOverride() const;

    // --- Drücken/Loslassen, Flash, Lernen (live_panel_looks.cpp) ------------------------------------------------
    /// Knopf eines Looks gedrückt bzw. losgelassen (Player::liveDown/liveUp: ein Flash-Look kehrt beim Loslassen zurück).
    bool press(int index);
    void release(int index);
    /// Wie der Look ausgelöst wird (umschalten/flashen, Zeit zum Zurückblenden; leer = Zeit des Looks).
    bool setLookTrigger(int index, project::LookTrigger trigger, std::optional<double> returnSeconds);
    /// Das Rechtsklick-Menü eines Looks (aufgebaut, nicht gezeigt; der Aufrufer besitzt es).
    [[nodiscard]] QMenu* lookMenu(int index);
    /// Gelernte Quelle für eine Live-Funktion (dieselbe Belegung wie im Reiter "Steuerung").
    using LearnHandler = std::function<void(runtime::LiveFunction, const LearnedSource&)>;
    /// Zeigt die Zeile "Steuerung lernen" mit einem Lernen-Knopf je Funktion (MIDI und DMX).
    void setLearning(std::shared_ptr<midi::MidiStore> midi, std::shared_ptr<const input::DmxStore> dmx, LearnHandler handler);

Q_SIGNALS:
    void failed(const QString& message);
    /// Der Knopf "Zurück zum Editor" (oder Esc).
    void leaveRequested();

private:
    [[nodiscard]] static QString styleFor(const QString& color);
    [[nodiscard]] QString buttonText(int index) const;
    void askReturnSeconds(int index);

    runtime::Player& player_;
    QGridLayout* grid_{nullptr};
    QWidget* gridHost_{nullptr};
    std::vector<QPushButton*> buttons_;
    std::vector<QShortcut*> digitKeys_;  // Tasten 1 bis 9 (aus, solange die Cuelist läuft)
    QLabel* nowLabel_{nullptr};
    QLabel* nextLabel_{nullptr};
    QProgressBar* progress_{nullptr};
    QPushButton* goButton_{nullptr};
    QCheckBox* useGo_{nullptr};
    QCheckBox* customFade_{nullptr};
    QDoubleSpinBox* fade_{nullptr};
    QWidget* learnRow_{nullptr};
};

}  // namespace whitehole::studio
