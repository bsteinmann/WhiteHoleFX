// shape_preview.hpp: 3D-Vorschau einer einzelnen Fixture-Form mit Testmuster.
//
// Warum es diese Datei gibt: Beim Anlegen einer Bauform (Matrix, Kreis, Säule ...) will man sofort sehen, wie sie
// aussieht und wie das Kabel läuft, noch bevor ein Fixture im Raum steht. Das Widget baut aus der Form eine eigene
// kleine LED-Karte (ohne Projekt, ohne Player) und zeichnet sie durch die drehbare 3D-Ansicht. Ohne Test sind die LEDs
// entlang des Kabels immer heller (die erste ist weiß); mit "Test" läuft ein Muster darüber: Lauflicht in
// Kabelreihenfolge, ein Balken entlang x, y oder z oder die Farbfolge Rot, Grün, Blau, Weiß.
#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QTimer>
#include <QWidget>

#include <optional>

#include "whitehole/mapping/fixture.hpp"
#include "whitehole/mapping/led_map.hpp"
#include "whitehole/preview/view3d.hpp"
#include "whitehole/runtime/test_pattern.hpp"

class QComboBox;
class QPushButton;

namespace whitehole::studio {

class PreviewWidget;

class ShapePreview : public QWidget {
    Q_OBJECT

public:
    explicit ShapePreview(QWidget* parent = nullptr);

    /// Zeigt diese Form. Ungültige Formen zeigen einen Hinweis statt eines Bildes.
    void setShape(const mapping::FixtureShape& shape);
    /// Läuft das Testmuster gerade?
    [[nodiscard]] bool testing() const noexcept { return testing_; }
    void setTesting(bool on);
    /// Die Art des Testmusters (0 Lauflicht, 1 Balken x, 2 Balken y, 3 Balken z, 4 Farbfolge).
    void setPatternIndex(int index);
    [[nodiscard]] int patternIndex() const;

    /// Anzahl der LEDs der gezeigten Karte (0 bei ungültiger Form), zum Prüfen.
    [[nodiscard]] std::size_t ledCount() const noexcept { return map_ ? map_->size() : 0U; }
    /// Das zuletzt gezeichnete Bild (leer, wenn nichts gezeichnet wurde).
    [[nodiscard]] const QImage& image() const noexcept { return last_; }
    /// Zeichnet jetzt (für den Rauchtest auch bei unsichtbarem Widget).
    bool renderNow();
    /// Rechnet einen Schritt des Tests weiter (für den Rauchtest; sonst macht das der Zeitgeber).
    void tick();

    [[nodiscard]] QSize sizeHint() const override { return {420, 320}; }

    /// Rauchtest für die CI: Form setzen, zeichnen, Test starten, jede Art zeichnen, stoppen. Leer bei Erfolg.
    QString exercise();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    [[nodiscard]] runtime::TestPattern currentPattern() const;
    [[nodiscard]] std::vector<ColorRgba> colors() const;

    PreviewWidget* view_;
    QComboBox* pattern_;
    QPushButton* testButton_;
    QTimer timer_;
    QElapsedTimer clock_;
    preview::Camera3d camera_;
    std::optional<mapping::LedMap> map_;
    QImage last_;
    bool testing_{false};
};

}  // namespace whitehole::studio
