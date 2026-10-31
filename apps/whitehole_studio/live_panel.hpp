// live_panel.hpp: die Bedienfläche für den Live-Betrieb.
//
// Warum es das gibt: Auf der Bühne muss ein Look mit einem Klick oder einer Taste abrufbar sein, ohne
// Editor, Parameter und Menüs. Dieses Panel zeigt für jeden gespeicherten Look einen großen Knopf (Tasten
// 1-9 für die ersten neun), dazu GO, die Überblendzeit und einen Balken für den laufenden Fade.
//
// Zwei Arten zu schalten:
//   - Direkt (Standard): ein Klick blendet sofort zum Look über - ein Handgriff, kaum Fehlbedienung.
//   - Mit GO: ein Klick lädt den Look nur vor ("Next", orange); GO (Leertaste) startet dann die Überblendung.
//     So kann der Shader vorher bereitgestellt werden, und der Fade selbst läuft ohne Ruckler.
// Farben: grün = läuft, orange = vorgeladen, gelb = blendet gerade ein.
//
// Die Logik steckt im Player (liveTrigger/liveArm/liveGo); das Panel ruft nur auf und zeigt liveStatus().
#pragma once

#include <QWidget>

#include <optional>
#include <vector>

#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QProgressBar;
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

Q_SIGNALS:
    void failed(const QString& message);
    /// Der Knopf "Zurück zum Editor" (oder Esc).
    void leaveRequested();

private:
    [[nodiscard]] std::optional<double> fadeOverride() const;
    [[nodiscard]] static QString styleFor(const QString& color);

    runtime::Player& player_;
    QGridLayout* grid_{nullptr};
    QWidget* gridHost_{nullptr};
    std::vector<QPushButton*> buttons_;
    QLabel* nowLabel_{nullptr};
    QLabel* nextLabel_{nullptr};
    QProgressBar* progress_{nullptr};
    QPushButton* goButton_{nullptr};
    QCheckBox* useGo_{nullptr};
    QCheckBox* customFade_{nullptr};
    QDoubleSpinBox* fade_{nullptr};
};

}  // namespace whitehole::studio
