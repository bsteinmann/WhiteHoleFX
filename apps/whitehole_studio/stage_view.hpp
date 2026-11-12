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

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <optional>
#include <utility>
#include <vector>

#include "whitehole/mapping/fixture.hpp"
#include "whitehole/preview/gizmo.hpp"
#include "whitehole/preview/view3d.hpp"
#include "whitehole/preview/wiring.hpp"
#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QLabel;
class QPainter;

namespace whitehole::studio {

class PreviewWidget;

/// Farbe der Achse im Griff (0 x rot, 1 y grün, 2 z blau); `bright` beim Überfahren und Ziehen.
[[nodiscard]] QColor gizmoAxisColor(int axis, bool bright);

class StageView : public QWidget {
    Q_OBJECT

public:
    /// `compact`: ohne Knopfleiste (Perspektive/Front/Seite/Oben), niedrig, für neben dem Formular.
    StageView(runtime::Player& player, bool compact, QWidget* parent = nullptr);

    /// Welche Fixtures kräftig gezeichnet werden (leer: alle).
    void setSelection(std::vector<mapping::FixtureId> ids);
    /// Zeichnet neu, wenn die Ansicht sichtbar ist (sonst beim nächsten Anzeigen).
    void refresh();
    /// Testansicht (Reiter Test): Ist ein Testbild aktiv, zeigt die Ansicht dessen Farben und lässt es laufen
    /// (die Ausgabe kann dabei aus sein); sonst wie sonst auch die Anlage.
    void setTestPreview(bool on);
    /// Das Testbild wurde ein-, aus- oder umgeschaltet: Zeitzähler neu starten und neu zeichnen.
    void testPatternChanged();
    /// Wahr, wenn das zuletzt gezeichnete Bild die Farben eines Testbilds zeigt (für den Rauchtest).
    [[nodiscard]] bool showsTestPattern() const noexcept { return drewTestPattern_; }

    [[nodiscard]] const preview::Camera3d& camera() const noexcept { return camera_; }

    /// Wählt die LED unter dem Bildpunkt (Widget-Koordinaten der Vorschau) und zeigt ihre Adresse. Wahr, wenn eine
    /// LED getroffen wurde. Ein Klick in der Vorschau ruft das auf.
    bool pickAt(QPointF position);
    /// Kabelweg, Pfeile, Start-LED und Nummern ein- oder ausblenden.
    void setWiringVisible(bool on);
    [[nodiscard]] QString infoText() const;

    /// Achsen-Griff am ersten gewählten Fixture (Pfeile x rot, y grün, z blau): ziehen verschiebt alle gewählten Fixtures
    /// entlang der Achse. Nur in der großen Ansicht und nicht im Testbild. Aus mit false.
    void setGizmoEnabled(bool on);
    /// Der Pfeil der Achse `axis` (0 x, 1 y, 2 z) im Widget als Anfang und Spitze; leer ohne Griff (für den Rauchtest).
    [[nodiscard]] std::optional<std::pair<QPointF, QPointF>> gizmoAxisInWidget(int axis) const;
    /// Rauchtest für die CI: zeichnet, dreht, wechselt die feste Ansicht. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    /// Der Griff hat Fixtures verschoben (das Projekt hat sich geändert): Layout-Liste und Vorschau nachziehen.
    void fixturesMoved();
    /// Der Griff konnte nicht verschieben (Meldung für die Statuszeile).
    void failed(const QString& message);
    /// Eine LED wurde angeklickt: ihr Fixture (`extend`: mit Strg, zur Auswahl dazu). Die Test-Seite wählt damit Fixtures.
    void fixtureClicked(mapping::FixtureId id, bool extend);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    /// Zeichnet jetzt, auch wenn das Widget nicht sichtbar ist. Wahr, wenn ein Bild entstanden ist.
    bool renderNow();
    void setPreset(preview::ViewPreset preset);
    void paintWiring(QPainter& painter) const;
    [[nodiscard]] QString describeLed(const preview::PickedLed& picked) const;

    // Achsen-Griff (stage_view_gizmo.cpp)
    void updateGizmo();
    void paintGizmo(QPainter& painter) const;
    bool gizmoGrab(QPointF position);
    void gizmoDrag(QPointF position, Qt::KeyboardModifiers modifiers);
    void gizmoRelease();
    void gizmoHover(QPointF position);
    [[nodiscard]] std::optional<int> gizmoAxisAt(QPointF position) const;
    // Drehringe (stage_view_rings.cpp), Teil des Achsen-Griffs
    void paintRings(QPainter& painter) const;
    bool ringGrab(QPointF position);
    void ringDrag(QPointF position, Qt::KeyboardModifiers modifiers);
    [[nodiscard]] std::optional<int> ringAt(QPointF position) const;
    [[nodiscard]] QPointF imageScale() const;  // Bildpunkte je Widgetpunkt (x, y)
    QString exerciseGizmo();
    QString exerciseRings();  // Teil von exerciseGizmo (stage_view_rings.cpp)

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
    bool gizmoEnabled_{false};
    std::optional<preview::GizmoGeometry> gizmo_;  // in Bildpunkten des zuletzt gezeichneten Bildes
    int gizmoActive_{-1};                          // gerade gezogene Achse, sonst -1
    int gizmoHover_{-1};                           // Achse unter der Maus, sonst -1
    QPointF gizmoLast_;
    float gizmoRaw_{0.0F};                         // bisher gezogene Strecke ohne Einrasten (Welteinheiten)
    float gizmoApplied_{0.0F};                     // davon schon verschoben (eingerastet)
    int ringActive_{-1};                           // gerade gedrehter Ring (Achse), sonst -1
    int ringHover_{-1};                            // Ring unter der Maus, sonst -1
    float ringLast_{0.0F};                         // letzter Ringwinkel unter der Maus (Grad)
    float ringRaw_{0.0F};                          // bisher gedreht ohne Einrasten (Grad)
    float ringApplied_{0.0F};                      // davon schon angewendet (eingerastet)
    bool showWiring_{true};
    bool testPreview_{false};
    bool drewTestPattern_{false};
    QTimer testTimer_;
    QElapsedTimer testClock_;
};

}  // namespace whitehole::studio
