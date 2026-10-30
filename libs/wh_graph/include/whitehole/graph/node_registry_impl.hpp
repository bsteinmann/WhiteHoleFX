// node_registry_impl.hpp: konkretes Verzeichnis aller Node-Typen.
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/graph/node_registry.hpp"

namespace whitehole::graph {

/// Implementiert Lesen und Eintragen. Wird beim Start befüllt (eingebaute Nodes,
/// danach Plugins) und anschließend nur noch gelesen; Eintragen ist nicht
/// thread-sicher. Zeiger aus find()/all() bleiben bei späteren Registrierungen gültig.
class NodeRegistry final : public INodeRegistry, public INodeRegistrar {
public:
    [[nodiscard]] const NodeDescriptor* find(std::string_view typeId) const override;

    /// Sortiert nach Typ-ID, damit Menüs und Tests deterministisch sind.
    [[nodiscard]] std::vector<const NodeDescriptor*> all() const override;

    /// Prüft die Beschreibung, bevor sie aufgenommen wird:
    /// - Typ-ID nicht leer und noch nicht vergeben (sonst AlreadyExists),
    /// - Port- und Parameterschlüssel sind gültige Bezeichner
    ///   ([A-Za-z_][A-Za-z0-9_]*) und innerhalb ihrer Liste eindeutig,
    /// - minValue <= maxValue.
    /// Verstöße ergeben InvalidArgument mit sprechender Meldung.
    Result<void> registerNode(NodeDescriptor descriptor) override;

private:
    std::map<std::string, NodeDescriptor, std::less<>> descriptors_;
};

}  // namespace whitehole::graph
