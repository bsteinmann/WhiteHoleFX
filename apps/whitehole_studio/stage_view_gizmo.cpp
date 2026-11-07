// stage_view_gizmo.cpp: Teil von StageView: der Achsen-Griff zum Verschieben von Fixtures in der 3D-Bühnenansicht.
//
// Warum eine eigene Datei: stage_view.cpp bleibt bei Bild, Kabelweg und Adresse; hier steht alles zum Griff: wo er
// gezeichnet wird, wie die Maus ihn trifft und wie ein Zug in Welteinheiten umgerechnet wird. Die Rechnung selbst
// (preview/gizmo.hpp) hat keinen Qt-Bezug und ist in den Unit-Tests abgedeckt; hier hängt nur Qt und der Player daran.
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

constexpr float kSnapStep = 0.1F;  // Einrasten beim Ziehen (Alt gedrückt: aus)

QColor axisColor(int axis, bool bright) {
    const QColor base = axis == 0 ? QColor{235, 70, 70} : axis == 1 ? QColor{80, 215, 90} : QColor{80, 140, 255};
    return bright ? base.lighter(140) : base;
}

}  // namespace

void StageView::setGizmoEnabled(bool on) {
    gizmoEnabled_ = on;
    gizmoActive_ = -1;
    gizmoHover_ = -1;
    updateGizmo();
    view_->update();
}

void StageView::updateGizmo() {
    gizmo_.reset();
    const project::Project* project = player_.project();
    if (!gizmoEnabled_ || testPreview_ || selection_.empty() || project == nullptr || imageOptions_.width == 0) {
        return;
    }
    const mapping::Fixture* fixture = project->layout.find(selection_.front());
    if (fixture == nullptr) {
        return;
    }
    const preview::Projector projector{camera_, preview::boundsOf(player_.ledMap()), imageOptions_.width,
                                       imageOptions_.height, imageOptions_.margin};
    gizmo_ = preview::gizmoFor(projector, fixture->origin);
}

std::optional<int> StageView::gizmoAxisAt(QPointF position) const {
    if (!gizmo_ || imageOptions_.width == 0 || view_->width() <= 0 || view_->height() <= 0) {
        return std::nullopt;
    }
    const float sx = static_cast<float>(imageOptions_.width) / static_cast<float>(view_->width());
    const float sy = static_cast<float>(imageOptions_.height) / static_cast<float>(view_->height());
    return preview::pickGizmoAxis(*gizmo_, static_cast<float>(position.x()) * sx, static_cast<float>(position.y()) * sy);
}

std::optional<std::pair<QPointF, QPointF>> StageView::gizmoAxisInWidget(int axis) const {
    if (!gizmo_ || axis < 0 || axis > 2 || imageOptions_.width == 0) {
        return std::nullopt;
    }
    const double sx = static_cast<double>(view_->width()) / imageOptions_.width;
    const double sy = static_cast<double>(view_->height()) / imageOptions_.height;
    const auto i = static_cast<std::size_t>(axis);
    return std::make_pair(QPointF{gizmo_->originX * sx, gizmo_->originY * sy}, QPointF{gizmo_->tipX[i] * sx, gizmo_->tipY[i] * sy});
}

bool StageView::gizmoGrab(QPointF position) {
    const std::optional<int> axis = gizmoAxisAt(position);
    if (!axis) {
        return false;
    }
    gizmoActive_ = *axis;
    gizmoHover_ = *axis;
    gizmoLast_ = position;
    gizmoRaw_ = 0.0F;
    gizmoApplied_ = 0.0F;
    view_->setCursor(Qt::ClosedHandCursor);
    view_->update();
    return true;
}

void StageView::gizmoDrag(QPointF position, Qt::KeyboardModifiers modifiers) {
    if (gizmoActive_ < 0 || !gizmo_ || view_->width() <= 0 || view_->height() <= 0) {
        return;
    }
    const float sx = static_cast<float>(imageOptions_.width) / static_cast<float>(view_->width());
    const float sy = static_cast<float>(imageOptions_.height) / static_cast<float>(view_->height());
    const QPointF delta = position - gizmoLast_;
    gizmoLast_ = position;
    gizmoRaw_ += preview::gizmoDragDistance(*gizmo_, gizmoActive_, static_cast<float>(delta.x()) * sx, static_cast<float>(delta.y()) * sy);
    const float target = modifiers.testFlag(Qt::AltModifier) ? gizmoRaw_ : preview::snapDistance(gizmoRaw_, kSnapStep);
    const float step = target - gizmoApplied_;
    if (step == 0.0F) {
        return;
    }
    Vec3 move{};
    (gizmoActive_ == 0 ? move.x : gizmoActive_ == 1 ? move.y : move.z) = step;
    const Result<void> r = player_.moveFixtures(selection_, move);
    if (!r.ok()) {
        Q_EMIT failed(form::fromStd(r.error().message));
        gizmoRelease();
        return;
    }
    gizmoApplied_ = target;
    Q_EMIT fixturesMoved();  // Layout-Liste und Vorschau nachziehen (zeichnet auch diese Ansicht neu)
}

void StageView::gizmoRelease() {
    gizmoActive_ = -1;
    view_->setCursor(Qt::ArrowCursor);
    view_->update();
}

void StageView::gizmoHover(QPointF position) {
    const int axis = gizmoAxisAt(position).value_or(-1);
    if (axis == gizmoHover_) {
        return;
    }
    gizmoHover_ = axis;
    view_->setCursor(axis >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    view_->update();
}

void StageView::paintGizmo(QPainter& painter) const {
    if (!gizmo_ || imageOptions_.width == 0 || imageOptions_.height == 0) {
        return;
    }
    const double sx = static_cast<double>(view_->width()) / imageOptions_.width;
    const double sy = static_cast<double>(view_->height()) / imageOptions_.height;
    const QPointF origin{gizmo_->originX * sx, gizmo_->originY * sy};
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (int axis = 0; axis < 3; ++axis) {
        const auto i = static_cast<std::size_t>(axis);
        const QPointF tip{gizmo_->tipX[i] * sx, gizmo_->tipY[i] * sy};
        const bool grabbable = preview::gizmoAxisPixels(*gizmo_, axis) >= preview::kGizmoMinPixels;
        const bool bright = axis == gizmoHover_ || axis == gizmoActive_;
        QColor color = axisColor(axis, bright);
        if (!grabbable) {
            color.setAlpha(90);  // zeigt zur Kamera: nur angedeutet, nicht greifbar
        }
        painter.setPen(QPen{color, bright ? 4.0 : 2.5, Qt::SolidLine, Qt::RoundCap});
        painter.drawLine(origin, tip);
        const double length = std::hypot(tip.x() - origin.x(), tip.y() - origin.y());
        if (length < 1.0) {
            continue;
        }
        const QPointF dir{(tip.x() - origin.x()) / length, (tip.y() - origin.y()) / length};
        const QPointF side{-dir.y(), dir.x()};
        QPolygonF head;
        head << QPointF{tip.x() + dir.x() * 9, tip.y() + dir.y() * 9}
             << QPointF{tip.x() + side.x() * 5, tip.y() + side.y() * 5} << QPointF{tip.x() - side.x() * 5, tip.y() - side.y() * 5};
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(head);
        painter.setPen(color);
        painter.drawText(QPointF{tip.x() + dir.x() * 18 - 4, tip.y() + dir.y() * 18 + 5}, QString{QStringLiteral("XYZ").at(axis)});
    }
    painter.setPen(QPen{Qt::white, 1.0});
    painter.setBrush(QColor{30, 30, 30});
    painter.drawEllipse(origin, 4.0, 4.0);
}

QString StageView::exerciseGizmo() {
    const project::Project* project = player_.project();
    if (project == nullptr || project->layout.fixtures().empty()) {
        return {};
    }
    const mapping::FixtureId id = project->layout.fixtures().front().id;
    setGizmoEnabled(true);
    setPreset(preview::ViewPreset::Perspective);
    setSelection({id});
    if (!renderNow() || !gizmo_) {
        return QStringLiteral("Bühnenansicht: der Achsen-Griff erscheint nicht");
    }
    // Leer gewählt: kein Griff
    setSelection({});
    if (!renderNow() || gizmo_) {
        return QStringLiteral("Bühnenansicht: der Achsen-Griff erscheint ohne Auswahl");
    }
    setSelection({id});
    renderNow();
    const auto x = gizmoAxisInWidget(0);
    if (!x) {
        return QStringLiteral("Bühnenansicht: der Achsen-Griff hat keine x-Achse");
    }
    const QPointF middle = (x->first + x->second) / 2.0;
    gizmoHover(middle);
    if (gizmoHover_ != 0 && gizmoAxisAt(middle).value_or(-1) != 0) {
        return QStringLiteral("Bühnenansicht: die x-Achse des Griffs wird nicht getroffen");
    }
    const Vec3 before = player_.project()->layout.find(id)->origin;
    int moved = 0;
    const QMetaObject::Connection counter = connect(this, &StageView::fixturesMoved, this, [&moved]() { ++moved; });
    const bool grabbed = gizmoGrab(middle);
    gizmoDrag(middle + (x->second - x->first), Qt::NoModifier);  // um einen ganzen Pfeil in Pfeilrichtung
    gizmoRelease();
    disconnect(counter);
    const Vec3 after = player_.project()->layout.find(id)->origin;
    if (!grabbed || moved != 1 || !(after.x > before.x) || after.y != before.y || after.z != before.z) {
        return QStringLiteral("Bühnenansicht: Ziehen am x-Pfeil verschiebt nur x nach rechts nicht");
    }
    // Zurück an die alte Stelle (der Test soll das Projekt nicht verändern)
    if (!player_.moveFixtures({id}, Vec3{before.x - after.x, 0.0F, 0.0F}).ok()) {
        return QStringLiteral("Bühnenansicht: Zurückschieben nach dem Griff-Test schlug fehl");
    }
    setSelection({});
    renderNow();
    return {};
}

}  // namespace whitehole::studio
