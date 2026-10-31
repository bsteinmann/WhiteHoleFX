// player_arrange.cpp: mehrere Fixtures zusammen bewegen, ausrichten, verteilen; Effektbereich setzen.
//
// Warum es diese Datei gibt: player_layout.cpp bearbeitet einzelne Fixtures. Das Anordnen im
// Layout-Editor wirkt auf eine Auswahl und muss genau ein Undo-Schritt sein; die Rechnung
// (Gesamtbox, gleiche Lücken) steht hier, getrennt von der Oberfläche, damit sie testbar ist.
#include <algorithm>
#include <cmath>

#include "whitehole/runtime/player.hpp"

namespace whitehole::runtime {

namespace {

Error invalid(std::string message) { return Error{ErrorCode::InvalidArgument, std::move(message)}; }

using Align = Player::Align;
using Axis = Player::Axis;

float component(const Vec3& v, Axis axis) noexcept {
    return axis == Axis::X ? v.x : (axis == Axis::Y ? v.y : v.z);
}

void setComponent(Vec3& v, Axis axis, float value) noexcept {
    (axis == Axis::X ? v.x : (axis == Axis::Y ? v.y : v.z)) = value;
}

/// Box eines Fixtures im Raum (Ursprung + lokale Box).
struct Placed {
    mapping::FixtureId id;
    Vec3 min;
    Vec3 max;
};

Result<std::vector<Placed>> collect(const project::Project& p, const std::vector<mapping::FixtureId>& ids) {
    std::vector<Placed> out;
    for (const mapping::FixtureId id : ids) {
        const mapping::Fixture* fixture = p.layout.find(id);
        if (fixture == nullptr) {
            return Error{ErrorCode::NotFound, "Fixture unbekannt"};
        }
        if (std::any_of(out.begin(), out.end(), [id](const Placed& placed) { return placed.id == id; })) {
            continue;  // doppelte Einträge zählen einmal
        }
        const mapping::Box local = mapping::localBox(fixture->shape);
        const Vec3& o = fixture->origin;
        out.push_back({id,
                       {o.x + local.min.x, o.y + local.min.y, o.z + local.min.z},
                       {o.x + local.max.x, o.y + local.max.y, o.z + local.max.z}});
    }
    return out;
}

Result<void> shift(project::Project& p, const Placed& placed, Axis axis, float delta) {
    const mapping::Fixture* fixture = p.layout.find(placed.id);
    Vec3 origin = fixture->origin;
    setComponent(origin, axis, component(origin, axis) + delta);
    return p.layout.move(placed.id, origin);
}

}  // namespace

Result<void> Player::moveFixtures(const std::vector<mapping::FixtureId>& ids, const Vec3& delta) {
    if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || !std::isfinite(delta.z)) {
        return invalid("Der Versatz muss eine endliche Zahl sein");
    }
    return editProject({}, [&](project::Project& p) -> Result<void> {
        const Result<std::vector<Placed>> placed = collect(p, ids);
        if (!placed.ok()) {
            return placed.error();
        }
        for (const Placed& item : placed.value()) {
            for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
                if (const float d = component(delta, axis); d != 0.0F) {
                    if (const Result<void> r = shift(p, item, axis, d); !r.ok()) {
                        return r;
                    }
                }
            }
        }
        return {};
    });
}

Result<void> Player::alignFixtures(const std::vector<mapping::FixtureId>& ids, Align mode) {
    if (ids.empty()) {
        return invalid("Keine Fixtures gewählt");
    }
    return editProject({}, [&](project::Project& p) -> Result<void> {
        const Result<std::vector<Placed>> collected = collect(p, ids);
        if (!collected.ok()) {
            return collected.error();
        }
        const std::vector<Placed>& items = collected.value();
        const Axis axis = (mode == Align::Left || mode == Align::HCenter || mode == Align::Right) ? Axis::X : Axis::Y;
        float lo = component(items.front().min, axis);
        float hi = component(items.front().max, axis);
        for (const Placed& item : items) {
            lo = std::min(lo, component(item.min, axis));
            hi = std::max(hi, component(item.max, axis));
        }
        for (const Placed& item : items) {
            const float itemLo = component(item.min, axis);
            const float itemHi = component(item.max, axis);
            float delta = 0.0F;
            switch (mode) {
                case Align::Left:
                case Align::Bottom: delta = lo - itemLo; break;
                case Align::Right:
                case Align::Top: delta = hi - itemHi; break;
                case Align::HCenter:
                case Align::VCenter: delta = (lo + hi) * 0.5F - (itemLo + itemHi) * 0.5F; break;
            }
            if (delta != 0.0F) {
                if (const Result<void> r = shift(p, item, axis, delta); !r.ok()) {
                    return r;
                }
            }
        }
        return {};
    });
}

Result<void> Player::distributeFixtures(const std::vector<mapping::FixtureId>& ids, Axis axis) {
    return editProject({}, [&](project::Project& p) -> Result<void> {
        Result<std::vector<Placed>> collected = collect(p, ids);
        if (!collected.ok()) {
            return collected.error();
        }
        std::vector<Placed> items = std::move(collected.value());
        if (items.size() < 3) {
            return invalid("Zum Verteilen braucht es mindestens drei Fixtures");
        }
        std::sort(items.begin(), items.end(), [axis](const Placed& a, const Placed& b) {
            return component(a.min, axis) + component(a.max, axis) < component(b.min, axis) + component(b.max, axis);
        });
        const float start = component(items.front().min, axis);
        const float end = component(items.back().max, axis);
        float used = 0.0F;
        for (const Placed& item : items) {
            used += component(item.max, axis) - component(item.min, axis);
        }
        const float gap = (end - start - used) / static_cast<float>(items.size() - 1);
        float cursor = start;
        for (const Placed& item : items) {
            const float delta = cursor - component(item.min, axis);
            if (delta != 0.0F) {
                if (const Result<void> r = shift(p, item, axis, delta); !r.ok()) {
                    return r;
                }
            }
            cursor += component(item.max, axis) - component(item.min, axis) + gap;
        }
        return {};
    });
}

Result<void> Player::setRegion(std::optional<mapping::Region> region) {
    if (region) {
        if (const Result<void> ok = mapping::validateRegion(*region); !ok.ok()) {
            return ok;
        }
    }
    return editProject("region", [&](project::Project& p) -> Result<void> {
        p.region = region;
        return {};
    });
}

}  // namespace whitehole::runtime
