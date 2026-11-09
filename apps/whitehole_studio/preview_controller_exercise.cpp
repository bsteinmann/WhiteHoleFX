// preview_controller_exercise.cpp: Rauchtest der Vorschau-Bedienung für die CI (`--exercise`).
//
// Warum es diese Datei gibt: Der Test spielt Klicks, Ziehen und Tasten durch, ohne dass jemand die
// Maus bewegt. Er steht getrennt vom Controller, damit dessen Datei klein bleibt.
#include <array>
#include <cmath>

#include "form_helpers.hpp"
#include "layout_panel.hpp"
#include "preview_controller.hpp"
#include "whitehole/preview/grid.hpp"

namespace whitehole::studio {

namespace {

QRectF toQRect(const preview::PixelRect& r) { return QRectF{r.left, r.top, r.width(), r.height()}; }

}  // namespace

QString PreviewController::exercise() {
    if (player_.project() == nullptr || player_.project()->layout.fixtures().empty()) {
        return QStringLiteral("Vorschau: kein Projekt mit Fixtures");
    }
    const auto render = [this]() -> QString {
        preview::PreviewOptions options;
        options.width = 800;
        options.height = 450;
        const auto image = player_.image(options);
        if (!image.ok()) {
            return QStringLiteral("Vorschau: kein Bild: %1").arg(form::fromStd(image.error().message));
        }
        imageRendered(image.value().frame);
        return {};
    };
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }
    const mapping::FixtureId id = player_.project()->layout.fixtures().front().id;
    const auto originOf = [this, id]() { return player_.project()->layout.find(id)->origin; };
    const Vec3 original = originOf();
    const std::size_t fixtureCount = player_.project()->layout.fixtures().size();

    panel_->setGrid(true, false, 1.0F);  // Teil 1 bis 5 ziehen ohne Einrasten

    // 1. Fixture anklicken und 40 Bildpunkte nach rechts ziehen.
    const QRectF box = toQRect(preview::toPixels(lastFrame_, preview::worldBox(*player_.project()->layout.find(id))));
    const QPointF center = box.center();
    const float dragScale = lastFrame_.scale;  // nach dem Loslassen passt sich der Ausschnitt neu an
    onPressed(center, Qt::NoModifier);
    if (panel_->selectedIds() != std::vector<mapping::FixtureId>{id}) {
        return QStringLiteral("Vorschau: Klick hat das Fixture nicht gewählt");
    }
    onDragged(center + QPointF{20.0, 0.0});
    onReleased(center + QPointF{40.0, 0.0});
    const float expected = 40.0F / dragScale;
    if (std::abs((originOf().x - original.x) - expected) > 0.11F || std::abs(originOf().y - original.y) > 0.11F) {
        return QStringLiteral("Vorschau: Ziehen verschob um %1 statt %2")
            .arg(static_cast<double>(originOf().x - original.x))
            .arg(static_cast<double>(expected));
    }
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }

    // 2. Gummiband über das ganze Bild wählt alle Fixtures.
    onPressed(QPointF{1.0, 1.0}, Qt::NoModifier);
    onDragged(QPointF{799.0, 449.0});
    onReleased(QPointF{799.0, 449.0});
    if (panel_->selectedIds().size() != fixtureCount) {
        return QStringLiteral("Vorschau: Gummiband wählte %1 von %2 Fixtures")
            .arg(panel_->selectedIds().size())
            .arg(fixtureCount);
    }

    // 3. Pfeiltaste schiebt das gewählte Fixture um 1.
    select({id});
    const float before = originOf().x;
    onNudged(1, 0, Qt::NoModifier);
    if (std::abs(originOf().x - before - 1.0F) > 1e-4F) {
        return QStringLiteral("Vorschau: Pfeiltaste schob um %1 statt 1").arg(static_cast<double>(originOf().x - before));
    }

    // 4. Region-Node anlegen: Ecke ziehen, Mitte verschieben, Pfeiltaste, Rückgängig.
    const Result<NodeId> added = player_.addNode("wh.transform.region", Vec2{0.0F, 0.0F});
    if (!added.ok()) {
        return QStringLiteral("Vorschau: Region-Node lässt sich nicht anlegen: %1").arg(form::fromStd(added.error().message));
    }
    const NodeId node = added.value();
    select({});
    setRegionNode(node);
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }
    const QRectF wall = wallRect();
    if (wall.width() < 20.0 || wall.height() < 20.0) {
        return QStringLiteral("Vorschau: Leinwand zu klein für den Bereichstest");
    }
    const auto at = [&wall](double fx, double fy) {
        return QPointF{wall.left() + fx * wall.width(), wall.top() + fy * wall.height()};
    };
    const auto region = [this]() { return nodeRegion().value_or(mapping::Region{-1.0F, -1.0F, -1.0F, -1.0F}); };
    const auto closeTo = [](float a, float b) { return std::abs(a - b) <= 0.02F; };
    if (region() != mapping::Region{0.0F, 0.0F, 0.5F, 0.5F}) {
        return QStringLiteral("Vorschau: neuer Region-Node hat nicht das Standard-Rechteck");
    }
    // Untere rechte Ecke (0,5|0,5) auf (0,8|0,7) ziehen: Breite 0,8, Höhe 0,7.
    onPressed(at(0.5, 0.5), Qt::NoModifier);
    onDragged(at(0.65, 0.6));
    onReleased(at(0.8, 0.7));
    if (!closeTo(region().w, 0.8F) || !closeTo(region().h, 0.7F) || !closeTo(region().x, 0.0F)) {
        return QStringLiteral("Vorschau: Eckgriff änderte das Rechteck nicht wie gezogen");
    }
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }
    // Mittelgriff (0,4|0,35) um 0,1 nach rechts ziehen.
    onPressed(at(0.4, 0.35), Qt::NoModifier);
    onReleased(at(0.5, 0.35));
    if (!closeTo(region().x, 0.1F) || !closeTo(region().w, 0.8F)) {
        return QStringLiteral("Vorschau: Mittelgriff verschob das Rechteck nicht");
    }
    select({});
    onNudged(-1, 0, Qt::NoModifier);  // ohne gewähltes Fixture schiebt die Pfeiltaste das Rechteck
    if (!closeTo(region().x, 0.09F)) {
        return QStringLiteral("Vorschau: Pfeiltaste verschob das Rechteck nicht");
    }
    // Jede Ziehbewegung und jeder Tastendruck ist ein eigener Schritt: Pfeiltaste, Mitte, Ecke zurück.
    for (const float expected : {0.1F, 0.0F}) {
        if (!player_.undo().ok() || !closeTo(region().x, expected)) {
            return QStringLiteral("Vorschau: Rückgängig stellte das Rechteck nicht her");
        }
    }
    if (!player_.redo().ok() || !player_.redo().ok() || !closeTo(region().x, 0.09F)) {
        return QStringLiteral("Vorschau: Wiederholen stellte das Rechteck nicht her");
    }
    setRegionNode(std::nullopt);
    select({});

    // 5. LED-Info beim Überfahren: mitten auf dem Fixture gibt es einen Treffer, weit daneben keinen.
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }
    const QPointF middle =
        toQRect(preview::toPixels(lastFrame_, preview::worldBox(*player_.project()->layout.find(id)))).center();
    if (!ledText(middle, 400.0F).contains(QStringLiteral("LED"))) {
        return QStringLiteral("Vorschau: Überfahren des Fixtures liefert keine LED-Info");
    }
    if (!ledText(QPointF{-5000.0, -5000.0}, 5.0F).isEmpty()) {
        return QStringLiteral("Vorschau: weit außerhalb darf es keine LED-Info geben");
    }

    // 6. Raster: Linien erscheinen im Overlay, und beim Ziehen rastet die linke untere Ecke ein.
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }
    const preview::Grid grid = preview::buildGrid(lastFrame_, 800.0F, 450.0F, 1.0F);
    if (grid.vertical.size() < 2 || grid.horizontal.size() < 2) {
        return QStringLiteral("Vorschau: Raster hat %1 senkrechte und %2 waagrechte Linien")
            .arg(grid.vertical.size())
            .arg(grid.horizontal.size());
    }
    panel_->setGrid(true, true, 1.0F);
    panel_->setFixtureSnap(false);  // dieser Teil prüft nur das Raster
    const QPointF grab = toQRect(preview::toPixels(lastFrame_, preview::worldBox(*player_.project()->layout.find(id)))).center();
    onPressed(grab, Qt::NoModifier);
    onDragged(grab + QPointF{13.0, 7.0});
    onReleased(grab + QPointF{26.0, 14.0});
    const mapping::Box snapped = preview::worldBox(*player_.project()->layout.find(id));
    const auto onGrid = [](float value) { return std::abs(value - preview::snapToGrid(value, 1.0F)) < 1e-3F; };
    if (!onGrid(snapped.min.x) || !onGrid(snapped.min.y)) {
        return QStringLiteral("Vorschau: nach dem Ziehen liegt die Ecke bei %1/%2, nicht auf dem Raster (Weite %3)")
            .arg(static_cast<double>(snapped.min.x))
            .arg(static_cast<double>(snapped.min.y))
            .arg(1.0);
    }

    // 7. Einrasten an anderen Fixtures: Kante/Mitte landet genau auf einer Linie des Nachbarn (braucht zwei Fixtures).
    panel_->setFixtureSnap(true);
    const mapping::Fixture* neighbour = nullptr;
    for (const mapping::Fixture& other : player_.project()->layout.fixtures()) {
        if (other.id != id) {
            neighbour = &other;
            break;
        }
    }
    if (neighbour == nullptr) {
        return {};
    }
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }
    const mapping::Box neighbourBox = preview::worldBox(*neighbour);
    const mapping::Box own = preview::worldBox(*player_.project()->layout.find(id));
    const float scale = lastFrame_.scale > 0.0F ? lastFrame_.scale : 1.0F;
    // Ziel: linke Kante 2 Bildpunkte neben der rechten Kante des Nachbarn
    const float wantedX = neighbourBox.max.x + 2.0F / scale;
    const QPointF start = toQRect(preview::toPixels(lastFrame_, own)).center();
    const QPointF target = start + QPointF{static_cast<double>((wantedX - own.min.x) * scale), 0.0};
    onPressed(start, Qt::NoModifier);
    onDragged(start + (target - start) / 2.0);
    onDragged(target);
    const bool guideShown = !guides_.empty();
    onReleased(target);
    const mapping::Box placed = preview::worldBox(*player_.project()->layout.find(id));
    const std::array<float, 3> mine{placed.min.x, (placed.min.x + placed.max.x) * 0.5F, placed.max.x};
    const std::array<float, 3> theirs{neighbourBox.min.x, (neighbourBox.min.x + neighbourBox.max.x) * 0.5F, neighbourBox.max.x};
    bool aligned = false;
    for (const float a : mine) {
        for (const float b : theirs) {
            aligned = aligned || std::abs(a - b) < 1e-3F;
        }
    }
    if (!aligned || !guideShown) {
        return QStringLiteral("Vorschau: Einrasten an Fixtures: Kante liegt bei %1 (Nachbar %2 bis %3), Hilfslinie %4")
            .arg(static_cast<double>(placed.min.x)).arg(static_cast<double>(neighbourBox.min.x)).arg(static_cast<double>(neighbourBox.max.x)).arg(guideShown);
    }
    return {};
}

}  // namespace whitehole::studio
