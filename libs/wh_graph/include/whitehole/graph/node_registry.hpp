// node_registry.hpp: Verzeichnis aller bekannten Node-Typen.
// Lesen und Eintragen sind getrennte Schnittstellen (Interface Segregation):
// Der Graph bekommt nur Leserechte, der Plugin-Loader nur das Eintragen.
#pragma once

#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_descriptor.hpp"

namespace whitehole::graph {

class INodeRegistry {
public:
    virtual ~INodeRegistry() = default;

    /// Liefert nullptr, wenn der Typ unbekannt ist.
    [[nodiscard]] virtual const NodeDescriptor* find(std::string_view typeId) const = 0;
    [[nodiscard]] virtual std::vector<const NodeDescriptor*> all() const = 0;
};

class INodeRegistrar {
public:
    virtual ~INodeRegistrar() = default;

    /// Schlägt mit ErrorCode::AlreadyExists fehl, wenn die Typ-ID schon vergeben ist.
    virtual Result<void> registerNode(NodeDescriptor descriptor) = 0;
};

}  // namespace whitehole::graph
