// stage_view.hpp: Bühnenansicht für das Stage Config: die Fixtures im Raum, drehbar.
//
// Warum es das gibt: Beim Einrichten (Matrix, Strip, Cube) will man sofort sehen, ob das, was man
// eingetragen hat, so im Raum liegt, wie gedacht: Wo beginnt der Strang, welche Richtung ist x, y, z,
// stimmen Größe und Lage? Die Hauptvorschau zeigt dagegen den laufenden Effekt und ist 2D-bearbeitbar.
// Diese Ansicht zeigt nur die Anlage: jedes Fixture in eigenem Farbton, heller entlang des Kabels, die
// erste LED weiß, dazu Rahmen und Achsenkreuz (x rot, y grün, z blau). Die gewählten Fixtures bleiben
// kräftig, die anderen werden dunkel. Sie gibt es zweimal: groß als Seite "Ansicht" des Fensters und klein
// im Fixtures-Tab neben dem Formular ("kompakt").
//
// Das Widget hält keinen Zustand des Projekts; es fragt den Player (stageImage3d) und zeichnet über ein
// PreviewWidget, das Drehen, Verschieben und Zoomen schon kann.
#pragma once

#include <QImage>
#include <QPointF>
#include <QString>
#include <QWidget>

#include <optional>
#include <vector>

#include "whitehole/mapping/fixture.hpp"
#include "whitehole/preview/view3d.hpp"
#include "whitehole/preview/wiring.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class PreviewWidget;
class QCheckBox;
class QLabel;
class QPainter;

class StageView : public QWidget {
    Q_OBJECT

public:
    /// `compact`: ohne Knopfleiste (Perspektive/Front/Seite/Oben), niedrig, für neben dem Formular.
    StageView(runtime::Player& player, bool compact, QWidget* parent = nullptr);

    /// Welche Fixtures kräftig gezeichnet werden (leer: alle).
    void setSelection(std::vector<mapping::FixtureId> ids);
    /// Zeichnet neu, wenn die Ansicht sichtbar ist (sonst beim nächsten Anzeigen).
    void refresh();

    [[nodiscard]] const preview::Camera3d& camera() const noexcept { return camera_; }

    /// Wählt die LED unter dem Bildpunkt (Widget-Koordinaten der Vorschau) und zeigt ihre Adresse. Wahr, wenn eine
    /// LED getroffen wurde. Ein Klick in der Vorschau ruft das auf.
    bool pickAt(QPointF position);
    /// Kabelweg, Pfeile, Start-LED und Nummern ein- oder ausblenden.
    void setWiringVisible(bool on);
    [[nodiscard]] QString infoText() const;

    /// Rauchtest für die CI: zeichnet, dreht, wechselt die feste Ansicht. Leer bei Erfolg.
    QString exercise();

protected:
    void showEvent(QShowEvent* event) override;

private:
    /// Zeichnet jetzt, auch wenn das Widget nicht sichtbar ist. Wahr, wenn ein Bild entstanden ist.
    bool renderNow();
    void setPreset(preview::ViewPreset preset);
    void paintWiring(QPainter& painter) const;
    [[nodiscard]] QString describeLed(const preview::PickedLed& picked) const;

    runtime::Player& player_;
    PreviewWidget* view_;
    preview::Camera3d camera_;
    std::vector<mapping::FixtureId> selection_;
    QImage last_;
    QCheckBox* wiringCheck_{nullptr};
    QLabel* info_{nullptr};
    std::vector<preview::WirePath> wires_;  // Kabelwege aller Fixtures im zuletzt gezeichneten Bild
    std::optional<preview::PickedLed> picked_;
    preview::PreviewOptions imageOptions_;  // Größe und Rand des zuletzt gezeichneten Bildes
    bool showWiring_{true};
};

}  // namespace whitehole::studio
