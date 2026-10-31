// preview_controller.hpp: Bedienung der Vorschau mit der Maus.
//
// Warum es diese Datei gibt: Die Vorschau zeigt nicht nur, sie ist auch eine Arbeitsfläche. Hier
// steht, was Mausklicks und Pfeiltasten bedeuten:
//  - Klick auf ein Fixture wählt es (Strg/Umschalt: dazu- oder abwählen), Ziehen verschiebt die
//    gewählten Fixtures gemeinsam, Pfeiltasten schieben um 1 (Umschalt 10, Strg 0,1).
//  - Ziehen auf leerer Fläche spannt einen Auswahlrahmen auf.
//  - Im Bereichsmodus (Knopf im Panel "Bereich") wird stattdessen der Effektbereich gezeichnet,
//    verschoben und an den Ecken geändert.
// Die Rechnung (Pixel <-> Welt, Treffer, Bereichsgeometrie) steckt in wh_preview/wh_mapping und
// ist dort getestet; der Controller verbindet sie mit Widget, Player und Layout-Panel.
//
// Während einer Ziehbewegung bleibt der Bildausschnitt eingefroren (`frozenFrame`), sonst würde
// sich die Ansicht unter der Maus verschieben, wenn die Box der Fixtures wächst oder schrumpft.
// Jede Ziehbewegung ist ein Undo-Schritt.
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <optional>
#include <vector>

#include "preview_widget.hpp"
#include "whitehole/mapping/region_edit.hpp"
#include "whitehole/preview/view.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class LayoutPanel;

class PreviewController : public QObject {
    Q_OBJECT

public:
    PreviewController(runtime::Player& player, PreviewWidget* widget, LayoutPanel* panel, QObject* parent = nullptr);

    /// Der Ausschnitt, den das nächste Bild verwenden soll (nur während einer Ziehbewegung gesetzt).
    [[nodiscard]] std::optional<preview::ViewFrame> frozenFrame() const { return frozen_; }
    /// Meldet den Ausschnitt des zuletzt gezeichneten Bildes und frischt die Rahmen auf.
    void imageRendered(const preview::ViewFrame& frame);
    /// Zeichnet Auswahl- und Bereichsrahmen neu (nach Auswahl- oder Projektänderung).
    void refreshOverlay();

    /// Rauchtest für die CI: Fixture ziehen, Gummiband, Pfeiltaste, Bereich zeichnen; danach alles
    /// zurücknehmen. Leer bei Erfolg.
    QString exercise();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);

private:
    enum class Mode { Idle, MoveFixtures, RubberBand, RegionDrag };

    void onPressed(QPointF position, Qt::KeyboardModifiers modifiers);
    void onDragged(QPointF position);
    void onReleased(QPointF position);
    void onNudged(int dx, int dy, Qt::KeyboardModifiers modifiers);
    void pressRegion(QPointF position);
    void applyDrag(QPointF position, bool force);
    void finish();
    void select(const std::vector<mapping::FixtureId>& ids);
    void report(const Result<void>& result);

    [[nodiscard]] QRectF wallRect() const;
    [[nodiscard]] QRectF regionRect(const mapping::Region& region) const;
    [[nodiscard]] QPointF toFraction(QPointF position) const;

    runtime::Player& player_;
    PreviewWidget* widget_;
    LayoutPanel* panel_;

    bool regionMode_{false};
    bool haveFrame_{false};
    preview::ViewFrame lastFrame_;
    std::optional<preview::ViewFrame> frozen_;

    Mode mode_{Mode::Idle};
    QPointF press_;
    bool moved_{false};
    Vec3 applied_;                                // bereits angewendeter Versatz der Ziehbewegung
    std::vector<mapping::FixtureId> dragged_;     // die Fixtures, die gezogen werden
    std::vector<mapping::FixtureId> base_;        // Auswahl vor dem Gummiband (bei Strg/Umschalt)
    std::optional<mapping::FixtureId> clicked_;   // Klick ohne Ziehen wählt nur dieses
    QRectF rubber_;
    mapping::Region regionStart_;
    mapping::RegionHandle handle_{mapping::RegionHandle::Move};
    bool newRegion_{false};
    QElapsedTimer throttle_;
};

}  // namespace whitehole::studio
