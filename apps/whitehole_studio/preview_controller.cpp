// preview_controller.cpp: Maus und Tastatur in der Vorschau, siehe preview_controller.hpp.
#include "preview_controller.hpp"

#include <QLineF>

#include <algorithm>
#include <cmath>

#include "form_helpers.hpp"
#include "layout_panel.hpp"

namespace whitehole::studio {

namespace {

constexpr qreal kMinDragPixels = 3.0;     // darunter gilt eine Bewegung als Wackeln beim Klicken
constexpr qreal kHandlePixels = 10.0;     // Reichweite der Eckgriffe des Bereichs
constexpr qint64 kThrottleMs = 30;        // höchstens so oft wird während des Ziehens geändert

float snapTenth(qreal value) { return static_cast<float>(std::round(value * 10.0) / 10.0); }

bool contains(const std::vector<mapping::FixtureId>& ids, mapping::FixtureId id) {
    return std::ranges::find(ids, id) != ids.end();
}

QRectF toQRect(const preview::PixelRect& r) { return QRectF{r.left, r.top, r.width(), r.height()}; }

}  // namespace

PreviewController::PreviewController(runtime::Player& player, PreviewWidget* widget, LayoutPanel* panel,
                                     QObject* parent)
    : QObject{parent}, player_{player}, widget_{widget}, panel_{panel} {
    connect(widget_, &PreviewWidget::mousePressed, this, &PreviewController::onPressed);
    connect(widget_, &PreviewWidget::mouseDragged, this, &PreviewController::onDragged);
    connect(widget_, &PreviewWidget::mouseReleased, this, &PreviewController::onReleased);
    connect(widget_, &PreviewWidget::nudged, this, &PreviewController::onNudged);
    connect(panel_, &LayoutPanel::selectionChanged, this, [this]() { refreshOverlay(); });
    connect(panel_->regionForm(), &RegionForm::drawModeChanged, this, [this](bool on) {
        regionMode_ = on;
        refreshOverlay();
    });
}

void PreviewController::imageRendered(const preview::ViewFrame& frame) {
    lastFrame_ = frame;
    haveFrame_ = true;
    refreshOverlay();
}

QRectF PreviewController::wallRect() const {
    mapping::Box flat = player_.wallBox();
    flat.max.z = flat.min.z;  // der Bereich liegt in der Ebene der Wand
    return toQRect(preview::toPixels(lastFrame_, flat));
}

QRectF PreviewController::regionRect(const mapping::Region& region) const {
    const QRectF wall = wallRect();
    return QRectF{wall.left() + region.x * wall.width(), wall.top() + region.y * wall.height(),
                  region.w * wall.width(), region.h * wall.height()};
}

QPointF PreviewController::toFraction(QPointF position) const {
    const QRectF wall = wallRect();
    return QPointF{(position.x() - wall.left()) / wall.width(), (position.y() - wall.top()) / wall.height()};
}

void PreviewController::refreshOverlay() {
    PreviewOverlay overlay;
    const project::Project* project = player_.project();
    if (haveFrame_ && project != nullptr) {
        for (const mapping::FixtureId id : panel_->selectedIds()) {
            if (const mapping::Fixture* fixture = project->layout.find(id)) {
                overlay.selected.push_back(toQRect(preview::toPixels(lastFrame_, preview::worldBox(*fixture))));
            }
        }
        overlay.wall = wallRect();
        if (project->region) {
            overlay.region = regionRect(*project->region);
        }
        overlay.editingRegion = regionMode_;
    }
    overlay.rubberBand = rubber_;
    widget_->setOverlay(overlay);
}

void PreviewController::select(const std::vector<mapping::FixtureId>& ids) {
    panel_->setSelectedIds(ids);
    refreshOverlay();
}

void PreviewController::report(const Result<void>& result) {
    if (!result.ok()) {
        Q_EMIT failed(form::fromStd(result.error().message));
    }
}

void PreviewController::onPressed(QPointF position, Qt::KeyboardModifiers modifiers) {
    if (!haveFrame_ || player_.project() == nullptr) {
        return;
    }
    press_ = position;
    moved_ = false;
    applied_ = Vec3{};
    clicked_.reset();
    frozen_ = lastFrame_;
    throttle_.start();
    player_.closeEditGroup();  // jede Ziehbewegung ist ein eigener Undo-Schritt
    if (regionMode_) {
        pressRegion(position);
        return;
    }

    std::vector<mapping::FixtureId> selection = panel_->selectedIds();
    const bool additive = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::ShiftModifier);
    const auto hit = preview::fixtureAt(player_.project()->layout, lastFrame_, position.x(), position.y());
    if (hit) {
        const bool selected = contains(selection, *hit);
        if (additive && selected) {
            std::erase(selection, *hit);
            select(selection);
            finish();
            return;
        }
        if (additive) {
            selection.push_back(*hit);
        } else if (!selected) {
            selection = {*hit};
        } else if (selection.size() > 1) {
            clicked_ = *hit;  // ohne Ziehen bleibt nur dieses gewählt
        }
        select(selection);
        dragged_ = selection;
        mode_ = Mode::MoveFixtures;
        return;
    }
    base_ = additive ? selection : std::vector<mapping::FixtureId>{};
    if (!additive) {
        select({});
    }
    rubber_ = QRectF{position, QSizeF{}};
    mode_ = Mode::RubberBand;
}

void PreviewController::pressRegion(QPointF position) {
    const QRectF wall = wallRect();
    if (wall.width() < 2.0 || wall.height() < 2.0) {
        Q_EMIT failed(tr("Die Wand hat keine Ausdehnung, ein Bereich lässt sich nicht zeichnen"));
        finish();
        return;
    }
    mode_ = Mode::RegionDrag;
    newRegion_ = true;
    const auto& region = player_.project()->region;
    if (!region) {
        return;
    }
    regionStart_ = *region;
    const QRectF rect = regionRect(*region);
    struct Corner {
        QPointF point;
        mapping::RegionHandle handle;
    };
    for (const Corner& corner : {Corner{rect.topLeft(), mapping::RegionHandle::TopLeft},
                                 Corner{rect.topRight(), mapping::RegionHandle::TopRight},
                                 Corner{rect.bottomLeft(), mapping::RegionHandle::BottomLeft},
                                 Corner{rect.bottomRight(), mapping::RegionHandle::BottomRight}}) {
        if (QLineF{corner.point, position}.length() <= kHandlePixels) {
            handle_ = corner.handle;
            newRegion_ = false;
            return;
        }
    }
    if (rect.contains(position)) {
        handle_ = mapping::RegionHandle::Move;
        newRegion_ = false;
    }
}

void PreviewController::applyDrag(QPointF position, bool force) {
    if (!force && throttle_.isValid() && throttle_.elapsed() < kThrottleMs) {
        return;
    }
    const QPointF delta = position - press_;
    if (!moved_ && std::hypot(delta.x(), delta.y()) < kMinDragPixels) {
        return;
    }
    if (mode_ == Mode::MoveFixtures) {
        moved_ = true;
        const float scale = lastFrame_.scale > 0.0F ? lastFrame_.scale : 1.0F;
        const Vec3 total{snapTenth(delta.x() / scale), snapTenth(-delta.y() / scale), 0.0F};
        const Vec3 step{total.x - applied_.x, total.y - applied_.y, 0.0F};
        if (step.x == 0.0F && step.y == 0.0F) {
            return;
        }
        const Result<void> r = player_.moveFixtures(dragged_, step);
        report(r);
        if (r.ok()) {
            applied_ = total;
            Q_EMIT edited();
        }
    } else if (mode_ == Mode::RegionDrag) {
        moved_ = true;
        const QPointF from = toFraction(press_);
        const QPointF to = toFraction(position);
        const mapping::Region region =
            newRegion_ ? mapping::regionFromCorners(static_cast<float>(from.x()), static_cast<float>(from.y()),
                                                    static_cast<float>(to.x()), static_cast<float>(to.y()))
                       : mapping::dragRegion(regionStart_, handle_, static_cast<float>(to.x() - from.x()),
                                             static_cast<float>(to.y() - from.y()));
        report(player_.setRegion(region));
        panel_->regionForm()->rebuild();
        Q_EMIT edited();
    } else if (mode_ == Mode::RubberBand) {
        moved_ = true;
        rubber_ = QRectF{press_, position}.normalized();
        std::vector<mapping::FixtureId> selection = base_;
        const preview::PixelRect rect{static_cast<float>(rubber_.left()), static_cast<float>(rubber_.top()),
                                      static_cast<float>(rubber_.right()), static_cast<float>(rubber_.bottom())};
        for (const mapping::FixtureId id : preview::fixturesInRect(player_.project()->layout, lastFrame_, rect)) {
            if (!contains(selection, id)) {
                selection.push_back(id);
            }
        }
        select(selection);
    }
    throttle_.restart();
}

void PreviewController::onDragged(QPointF position) {
    if (mode_ != Mode::Idle) {
        applyDrag(position, false);
    }
}

void PreviewController::onReleased(QPointF position) {
    if (mode_ != Mode::Idle) {
        applyDrag(position, true);
    }
    finish();
}

void PreviewController::finish() {
    if (mode_ == Mode::MoveFixtures && !moved_ && clicked_) {
        select({*clicked_});
    }
    const bool changed = moved_ && (mode_ == Mode::MoveFixtures || mode_ == Mode::RegionDrag);
    mode_ = Mode::Idle;
    rubber_ = QRectF{};
    moved_ = false;
    frozen_.reset();
    player_.closeEditGroup();
    if (changed) {
        panel_->refreshValues();
        Q_EMIT edited();
    } else {
        refreshOverlay();
    }
}

void PreviewController::onNudged(int dx, int dy, Qt::KeyboardModifiers modifiers) {
    if (player_.project() == nullptr) {
        return;
    }
    player_.closeEditGroup();
    const bool coarse = modifiers.testFlag(Qt::ShiftModifier);
    if (regionMode_ && player_.project()->region) {
        const float step = coarse ? 0.1F : 0.01F;
        const mapping::Region moved = mapping::dragRegion(*player_.project()->region, mapping::RegionHandle::Move,
                                                          static_cast<float>(dx) * step, static_cast<float>(-dy) * step);
        report(player_.setRegion(moved));
        panel_->regionForm()->rebuild();
        refreshOverlay();
        Q_EMIT edited();
        return;
    }
    const std::vector<mapping::FixtureId> selection = panel_->selectedIds();
    if (selection.empty()) {
        return;
    }
    const float step = coarse ? 10.0F : (modifiers.testFlag(Qt::ControlModifier) ? 0.1F : 1.0F);
    report(player_.moveFixtures(selection, Vec3{static_cast<float>(dx) * step, static_cast<float>(dy) * step, 0.0F}));
    player_.closeEditGroup();
    panel_->refreshValues();
    Q_EMIT edited();
}

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

    // 4. Bereich zeichnen und verschieben.
    if (const QString note = render(); !note.isEmpty()) {
        return note;
    }
    panel_->regionForm()->setDrawMode(true);
    const QRectF wall = wallRect();
    if (wall.width() < 20.0 || wall.height() < 20.0) {
        return QStringLiteral("Vorschau: Wand zu klein für den Bereichstest");
    }
    const auto at = [&wall](double fx, double fy) {
        return QPointF{wall.left() + fx * wall.width(), wall.top() + fy * wall.height()};
    };
    onPressed(at(0.1, 0.1), Qt::NoModifier);
    onDragged(at(0.4, 0.4));
    onReleased(at(0.6, 0.6));
    const auto region = player_.project()->region;
    if (!region || std::abs(region->x - 0.1F) > 0.02F || std::abs(region->w - 0.5F) > 0.02F) {
        return QStringLiteral("Vorschau: Bereich wurde nicht wie gezeichnet angelegt");
    }
    onPressed(at(0.35, 0.35), Qt::NoModifier);  // Mitte des Bereichs: verschieben
    onReleased(at(0.45, 0.35));
    const auto moved = player_.project()->region;
    if (!moved || std::abs(moved->x - 0.2F) > 0.02F || std::abs(moved->w - 0.5F) > 0.02F) {
        return QStringLiteral("Vorschau: Bereich wurde nicht verschoben");
    }
    panel_->regionForm()->setDrawMode(false);

    // 5. Alles zurücknehmen: Ziehen, Pfeiltaste, Bereich zeichnen, Bereich verschieben = 4 Schritte.
    for (int i = 0; i < 4; ++i) {
        if (!player_.undo().ok()) {
            return QStringLiteral("Vorschau: Rückgängig scheiterte nach %1 Schritten").arg(i);
        }
    }
    panel_->rebuild();
    select({});
    if (originOf() != original || player_.project()->region.has_value()) {
        return QStringLiteral("Vorschau: Rückgängig stellte den Ausgangszustand nicht her");
    }
    return {};
}

}  // namespace whitehole::studio
