// stage_view_rings.cpp: Teil von StageView: die Drehringe des Achsen-Griffs (Fixtures in der 3D-Bühnenansicht drehen).
//
// Warum eine eigene Datei: stage_view_gizmo.cpp zeigt und bewegt die Pfeile; die Ringe haben eigene Zustände (welcher
// Ring, letzter Winkel, eingerastete Summe) und sollen die Datei nicht über die Zeilengrenze heben. Die Rechnung
// (Ringe im Bild, Treffer, Winkel auf dem Ring) steht in preview/gizmo.hpp und ist getestet. Hier hängen nur Qt und
// der Player dran: Der Winkel unter der Maus ändert sich, die Differenz dreht die gewählten Fixtures um die Weltachse.
#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

#include "form_helpers.hpp"
#include "preview_widget.hpp"
#include "stage_view.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::studio {

namespace {

constexpr float kRotateSnapDegrees = 5.0F;  // Einrasten beim Drehen (Alt gedrückt: aus)

}  // namespace

QPointF StageView::imageScale() const {
    if (view_->width() <= 0 || view_->height() <= 0) {
        return {1.0, 1.0};
    }
    return {static_cast<double>(imageOptions_.width) / view_->width(), static_cast<double>(imageOptions_.height) / view_->height()};
}

std::optional<int> StageView::ringAt(QPointF position) const {
    if (!gizmo_ || imageOptions_.width == 0) {
        return std::nullopt;
    }
    const QPointF scale = imageScale();
    return preview::pickGizmoRing(*gizmo_, static_cast<float>(position.x() * scale.x()), static_cast<float>(position.y() * scale.y()));
}

bool StageView::ringGrab(QPointF position) {
    const std::optional<int> ring = ringAt(position);
    if (!ring) {
        return false;
    }
    const QPointF scale = imageScale();
    ringActive_ = *ring;
    ringHover_ = *ring;
    ringLast_ = preview::gizmoRingAngle(*gizmo_, *ring, static_cast<float>(position.x() * scale.x()), static_cast<float>(position.y() * scale.y()));
    ringRaw_ = 0.0F;
    ringApplied_ = 0.0F;
    view_->setCursor(Qt::ClosedHandCursor);
    view_->update();
    return true;
}

void StageView::ringDrag(QPointF position, Qt::KeyboardModifiers modifiers) {
    if (ringActive_ < 0 || !gizmo_) {
        return;
    }
    const QPointF scale = imageScale();
    const float angle = preview::gizmoRingAngle(*gizmo_, ringActive_, static_cast<float>(position.x() * scale.x()),
                                                static_cast<float>(position.y() * scale.y()));
    ringRaw_ += preview::angleDelta(ringLast_, angle);
    ringLast_ = angle;
    const float target = modifiers.testFlag(Qt::AltModifier) ? ringRaw_ : preview::snapDistance(ringRaw_, kRotateSnapDegrees);
    const float step = target - ringApplied_;
    if (step == 0.0F) {
        return;
    }
    const Result<void> r = player_.rotateFixtures(selection_, static_cast<runtime::Player::Axis>(ringActive_), step);
    if (!r.ok()) {
        Q_EMIT failed(form::fromStd(r.error().message));
        gizmoRelease();
        return;
    }
    ringApplied_ = target;
    Q_EMIT fixturesMoved();  // Layout-Liste (Drehwinkel) und Vorschau nachziehen
}

void StageView::paintRings(QPainter& painter) const {
    if (!gizmo_ || imageOptions_.width == 0 || imageOptions_.height == 0) {
        return;
    }
    const QPointF scale = imageScale();
    for (int axis = 0; axis < 3; ++axis) {
        const auto a = static_cast<std::size_t>(axis);
        if (!gizmo_->ringVisible[a]) {
            continue;
        }
        QPolygonF line;
        for (int i = 0; i <= preview::kGizmoRingSegments; ++i) {
            const auto k = static_cast<std::size_t>(i);
            line << QPointF{gizmo_->ringX[a][k] / scale.x(), gizmo_->ringY[a][k] / scale.y()};
        }
        const bool usable = preview::gizmoRingMinorPixels(*gizmo_, axis) >= preview::kGizmoRingMinMinorPixels;
        const bool bright = axis == ringHover_ || axis == ringActive_;
        QColor color = gizmoAxisColor(axis, bright);
        color.setAlpha(!usable ? 60 : bright ? 255 : 170);  // von der Kante gesehen: nur angedeutet
        painter.setPen(QPen{color, bright ? 3.5 : 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        painter.setBrush(Qt::NoBrush);
        painter.drawPolyline(line);
    }
}

QString StageView::exerciseRings() {
    const project::Project* project = player_.project();
    if (project == nullptr || project->layout.fixtures().empty() || selection_.empty()) {
        return {};
    }
    const mapping::FixtureId id = selection_.front();
    renderNow();
    if (!gizmo_) {
        return QStringLiteral("Bühnenansicht: Drehringe fehlen (kein Griff)");
    }
    // Den am besten sichtbaren Ring nehmen (in der Perspektive ist mindestens einer eine breite Ellipse).
    int axis = 0;
    for (int candidate = 1; candidate < 3; ++candidate) {
        if (preview::gizmoRingMinorPixels(*gizmo_, candidate) > preview::gizmoRingMinorPixels(*gizmo_, axis)) {
            axis = candidate;
        }
    }
    if (preview::gizmoRingMinorPixels(*gizmo_, axis) < preview::kGizmoRingMinMinorPixels) {
        return QStringLiteral("Bühnenansicht: kein Drehring ist greifbar");
    }
    const QPointF scale = imageScale();
    const auto a = static_cast<std::size_t>(axis);
    // Start 45° (+ k·90°) auf dem Ring, Ende 30° weiter (Segmente à 7,5°); der Start liegt zwischen den Pfeilen und
    // nicht auf einem anderen Ring, damit der Treffer eindeutig ist.
    const auto at = [&](int segment) {
        const auto k = static_cast<std::size_t>(segment);
        return QPointF{gizmo_->ringX[a][k] / scale.x(), gizmo_->ringY[a][k] / scale.y()};
    };
    int start = -1;
    for (int base : {6, 18, 30, 42}) {
        if (!gizmoAxisAt(at(base)).has_value() && ringAt(at(base)) == std::optional<int>{axis}) {
            start = base;
            break;
        }
    }
    if (start < 0) {
        return QStringLiteral("Bühnenansicht: der Drehring wird nicht getroffen (Achse %1)").arg(axis);
    }
    const Vec3 before = project->layout.find(id)->rotation;
    const bool grabbed = gizmoGrab(at(start));
    gizmoDrag(at(start + 4), Qt::NoModifier);
    gizmoRelease();
    const Vec3 after = player_.project()->layout.find(id)->rotation;
    const Vec3 expected = mapping::rotationAfterWorldTurn(before, axis, 30.0F);
    const bool same = std::abs(after.x - expected.x) < 0.01F && std::abs(after.y - expected.y) < 0.01F && std::abs(after.z - expected.z) < 0.01F;
    // Zurück (der Test soll das Projekt nicht verändern)
    const bool restored = player_.setFixtureRotation(id, before).ok();
    if (!grabbed || !same || !restored) {
        return QStringLiteral("Bühnenansicht: Ziehen am Drehring dreht nicht um 30° (Achse %1, Winkel %2/%3/%4, erwartet %5/%6/%7)")
            .arg(axis).arg(after.x).arg(after.y).arg(after.z).arg(expected.x).arg(expected.y).arg(expected.z);
    }
    renderNow();
    return {};
}

}  // namespace whitehole::studio
