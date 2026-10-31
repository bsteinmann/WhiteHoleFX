// preview_controller.cpp: Maus und Tastatur in der Vorschau, siehe preview_controller.hpp.
#include "preview_controller.hpp"

#include <QLineF>

#include <algorithm>
#include <cmath>
#include <variant>

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
}

void PreviewController::setRegionNode(std::optional<NodeId> node) {
    regionNode_ = node;
    refreshOverlay();
}

std::optional<mapping::Region> PreviewController::nodeRegion() const {
    const project::Project* project = player_.project();
    if (!regionNode_ || project == nullptr) {
        return std::nullopt;
    }
    const graph::NodeInstance* node = project->graph.findNode(*regionNode_);
    if (node == nullptr || (node->typeId != "wh.transform.region" && node->typeId != "wh.output.canvas")) {
        return std::nullopt;
    }
    const auto read = [node](const char* key, float fallback) {
        const auto found = node->parameters.find(key);
        const double* value = found != node->parameters.end() ? std::get_if<double>(&found->second) : nullptr;
        return value != nullptr ? static_cast<float>(*value) : fallback;
    };
    return mapping::Region{read("left", 0.0F), read("top", 0.0F), read("width", 0.5F), read("height", 0.5F)};
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
        if (const auto region = nodeRegion()) {
            overlay.wall = wallRect();
            overlay.region = regionRect(*region);
            overlay.editingRegion = true;
        }
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
    if (pressRegion(position)) {
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

bool PreviewController::pressRegion(QPointF position) {
    const auto region = nodeRegion();
    if (!region) {
        return false;
    }
    const QRectF wall = wallRect();
    if (wall.width() < 2.0 || wall.height() < 2.0) {
        return false;
    }
    const QRectF rect = regionRect(*region);
    struct Grip {
        QPointF point;
        mapping::RegionHandle handle;
        qreal reach;
    };
    for (const Grip& grip : {Grip{rect.topLeft(), mapping::RegionHandle::TopLeft, kHandlePixels},
                             Grip{rect.topRight(), mapping::RegionHandle::TopRight, kHandlePixels},
                             Grip{rect.bottomLeft(), mapping::RegionHandle::BottomLeft, kHandlePixels},
                             Grip{rect.bottomRight(), mapping::RegionHandle::BottomRight, kHandlePixels},
                             Grip{rect.center(), mapping::RegionHandle::Move, kHandlePixels}}) {
        if (QLineF{grip.point, position}.length() <= grip.reach) {
            handle_ = grip.handle;
            regionStart_ = *region;
            mode_ = Mode::RegionDrag;
            return true;
        }
    }
    return false;  // sonst wie gewohnt: Fixture wählen oder Gummiband
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
        const mapping::Region region = mapping::dragRegion(regionStart_, handle_, static_cast<float>(to.x() - from.x()),
                                                           static_cast<float>(to.y() - from.y()));
        if (regionNode_) {
            report(player_.setRegionRect(*regionNode_, region));
        }
        refreshOverlay();
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
    const bool regionChanged = moved_ && mode_ == Mode::RegionDrag;
    mode_ = Mode::Idle;
    rubber_ = QRectF{};
    moved_ = false;
    frozen_.reset();
    player_.closeEditGroup();
    if (regionChanged) {
        Q_EMIT regionEdited();
    }
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
    if (const auto region = nodeRegion(); region && panel_->selectedIds().empty()) {
        const float step = coarse ? 0.1F : 0.01F;
        const mapping::Region moved = mapping::dragRegion(*region, mapping::RegionHandle::Move,
                                                          static_cast<float>(dx) * step, static_cast<float>(-dy) * step);
        report(player_.setRegionRect(*regionNode_, moved));
        player_.closeEditGroup();
        refreshOverlay();
        Q_EMIT regionEdited();
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

}  // namespace whitehole::studio
