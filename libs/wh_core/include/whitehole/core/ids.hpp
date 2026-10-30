// ids.hpp: typsichere Bezeichner für Nodes, Verbindungen und GPU-Ressourcen.
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace whitehole {

/// Zero-Cost-Wrapper um eine Ganzzahl. Der Tag-Typ macht jede ID-Art zu einem
/// eigenen Typ: Eine NodeId lässt sich nicht versehentlich als ConnectionId
/// übergeben. Der Wert 0 bedeutet "ungültig".
template <typename Tag, typename Rep = std::uint64_t>
class StrongId {
public:
    using rep_type = Rep;

    constexpr StrongId() noexcept = default;
    constexpr explicit StrongId(Rep value) noexcept : value_{value} {}

    [[nodiscard]] constexpr Rep value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool valid() const noexcept { return value_ != Rep{0}; }

    friend constexpr auto operator<=>(const StrongId&, const StrongId&) noexcept = default;

private:
    Rep value_{0};
};

/// Vergibt aufsteigende IDs ab 1.
/// Nicht thread-sicher: Ein Graph gehört immer genau einem Thread und wird
/// an andere nur als unveränderlicher Snapshot (Kopie) weitergegeben.
template <typename Id>
class IdGenerator {
public:
    [[nodiscard]] Id next() noexcept { return Id{++counter_}; }

    /// Beim Laden einer Projektdatei: Zähler hinter die größte vorhandene ID
    /// setzen. Ein kleinerer Wert setzt den Zähler nie zurück.
    void reserveUpTo(Id highest) noexcept {
        if (highest.value() > counter_) {
            counter_ = highest.value();
        }
    }

private:
    typename Id::rep_type counter_{0};
};

struct NodeIdTag {};
struct ConnectionIdTag {};

using NodeId = StrongId<NodeIdTag>;
using ConnectionId = StrongId<ConnectionIdTag>;

}  // namespace whitehole

template <typename Tag, typename Rep>
struct std::hash<whitehole::StrongId<Tag, Rep>> {
    [[nodiscard]] std::size_t operator()(const whitehole::StrongId<Tag, Rep>& id) const noexcept {
        return std::hash<Rep>{}(id.value());
    }
};
