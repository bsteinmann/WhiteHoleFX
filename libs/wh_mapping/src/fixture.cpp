#include "whitehole/mapping/fixture.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace whitehole::mapping {

namespace {

bool finite(float v) noexcept { return std::isfinite(v); }
bool finite(const Vec3& v) noexcept { return finite(v.x) && finite(v.y) && finite(v.z); }

Error invalid(const std::string& message) { return Error{ErrorCode::InvalidArgument, message}; }

/// Produkt mit Überlaufschutz: false, wenn es die Obergrenze überschreitet.
bool multiplyWithinLimit(std::uint64_t& total, std::uint64_t factor) noexcept {
    if (factor != 0U && total > kMaxLedsPerFixture / factor) {
        return false;
    }
    total *= factor;
    return true;
}

struct Validator {
    Result<void> operator()(const MatrixShape& m) const {
        if (m.columns == 0U || m.rows == 0U) {
            return invalid("Eine Matrix braucht mindestens eine Spalte und eine Zeile");
        }
        std::uint64_t total = 1;
        if (!multiplyWithinLimit(total, m.columns) || !multiplyWithinLimit(total, m.rows)) {
            return invalid("Die Matrix hat zu viele LEDs");
        }
        if (!finite(m.pitch.x) || !finite(m.pitch.y) || m.pitch.x <= 0.0F || m.pitch.y <= 0.0F) {
            return invalid("Der Rasterabstand der Matrix muss endlich und größer 0 sein");
        }
        return {};
    }

    Result<void> operator()(const StripShape& s) const {
        if (s.count == 0U || s.count > kMaxLedsPerFixture) {
            return invalid("Ein Strip braucht zwischen 1 und 4194304 LEDs");
        }
        if (!finite(s.start) || !finite(s.end)) {
            return invalid("Anfang und Ende des Strips müssen endliche Zahlen sein");
        }
        return {};
    }

    Result<void> operator()(const GridShape& g) const {
        if (g.nx == 0U || g.ny == 0U || g.nz == 0U) {
            return invalid("Ein Gitter braucht in jeder Richtung mindestens eine LED");
        }
        std::uint64_t total = 1;
        if (!multiplyWithinLimit(total, g.nx) || !multiplyWithinLimit(total, g.ny) ||
            !multiplyWithinLimit(total, g.nz)) {
            return invalid("Das Gitter hat zu viele LEDs");
        }
        if (!finite(g.pitch) || g.pitch.x <= 0.0F || g.pitch.y <= 0.0F || g.pitch.z <= 0.0F) {
            return invalid("Der Rasterabstand des Gitters muss endlich und größer 0 sein");
        }
        return {};
    }

    Result<void> operator()(const PointsShape& p) const {
        if (p.points.empty()) {
            return invalid("Eine Punktwolke braucht mindestens einen Punkt");
        }
        if (p.points.size() > kMaxLedsPerFixture) {
            return invalid("Die Punktwolke hat zu viele Punkte");
        }
        for (const Vec3& point : p.points) {
            if (!finite(point)) {
                return invalid("Alle Punkte müssen endliche Koordinaten haben");
            }
        }
        return {};
    }
};

struct Counter {
    std::uint64_t operator()(const MatrixShape& m) const noexcept {
        return std::uint64_t{m.columns} * m.rows;
    }
    std::uint64_t operator()(const StripShape& s) const noexcept { return s.count; }
    std::uint64_t operator()(const GridShape& g) const noexcept {
        return std::uint64_t{g.nx} * g.ny * g.nz;
    }
    std::uint64_t operator()(const PointsShape& p) const noexcept { return p.points.size(); }
};

struct BoxOf {
    Box operator()(const MatrixShape& m) const noexcept {
        return Box{Vec3{}, Vec3{static_cast<float>(m.columns) * m.pitch.x,
                                static_cast<float>(m.rows) * m.pitch.y, 0.0F}};
    }
    Box operator()(const StripShape& s) const noexcept {
        return Box{Vec3{std::min(s.start.x, s.end.x), std::min(s.start.y, s.end.y),
                        std::min(s.start.z, s.end.z)},
                   Vec3{std::max(s.start.x, s.end.x), std::max(s.start.y, s.end.y),
                        std::max(s.start.z, s.end.z)}};
    }
    Box operator()(const GridShape& g) const noexcept {
        return Box{Vec3{}, Vec3{static_cast<float>(g.nx) * g.pitch.x,
                                static_cast<float>(g.ny) * g.pitch.y,
                                static_cast<float>(g.nz) * g.pitch.z}};
    }
    Box operator()(const PointsShape& p) const noexcept {
        Box box{p.points.front(), p.points.front()};
        for (const Vec3& v : p.points) {
            box.min = Vec3{std::min(box.min.x, v.x), std::min(box.min.y, v.y), std::min(box.min.z, v.z)};
            box.max = Vec3{std::max(box.max.x, v.x), std::max(box.max.y, v.y), std::max(box.max.z, v.z)};
        }
        return box;
    }
};

struct PositionsOf {
    std::vector<Vec3> operator()(const MatrixShape& m) const {
        const std::uint64_t count = std::uint64_t{m.columns} * m.rows;
        std::vector<Vec3> out;
        out.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            const Cell2 cell = matrixCell(i, m.columns, m.rows, m.wiring);
            // Zeile 0 liegt oben, also bei der größten y-Koordinate.
            const float x = (static_cast<float>(cell.column) + 0.5F) * m.pitch.x;
            const float y = (static_cast<float>(m.rows - 1U - cell.row) + 0.5F) * m.pitch.y;
            out.push_back(Vec3{x, y, 0.0F});
        }
        return out;
    }

    std::vector<Vec3> operator()(const StripShape& s) const {
        std::vector<Vec3> out;
        out.reserve(s.count);
        const auto n = static_cast<float>(s.count);
        for (std::uint32_t i = 0; i < s.count; ++i) {
            const float t = (static_cast<float>(i) + 0.5F) / n;
            out.push_back(Vec3{s.start.x + t * (s.end.x - s.start.x),
                               s.start.y + t * (s.end.y - s.start.y),
                               s.start.z + t * (s.end.z - s.start.z)});
        }
        return out;
    }

    std::vector<Vec3> operator()(const GridShape& g) const {
        const std::uint64_t count = std::uint64_t{g.nx} * g.ny * g.nz;
        std::vector<Vec3> out;
        out.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            const Cell3 cell = gridCell(i, g.nx, g.ny, g.nz, g.wiring);
            out.push_back(Vec3{(static_cast<float>(cell.x) + 0.5F) * g.pitch.x,
                               (static_cast<float>(cell.y) + 0.5F) * g.pitch.y,
                               (static_cast<float>(cell.z) + 0.5F) * g.pitch.z});
        }
        return out;
    }

    std::vector<Vec3> operator()(const PointsShape& p) const { return p.points; }
};

}  // namespace

Result<void> validateShape(const FixtureShape& shape) { return std::visit(Validator{}, shape); }
std::uint64_t ledCount(const FixtureShape& shape) noexcept { return std::visit(Counter{}, shape); }
Box localBox(const FixtureShape& shape) noexcept { return std::visit(BoxOf{}, shape); }
std::vector<Vec3> localPositions(const FixtureShape& shape) { return std::visit(PositionsOf{}, shape); }

}  // namespace whitehole::mapping
