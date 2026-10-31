#include "whitehole/nodes/builtin_library.hpp"

#include "whitehole/nodes/builtin_nodes.hpp"

namespace whitehole::nodes {

Result<std::unique_ptr<BuiltinLibrary>> BuiltinLibrary::create() {
    auto library = std::make_unique<BuiltinLibrary>();
    library->registry = std::make_shared<graph::NodeRegistry>();
    library->dmx = std::make_shared<input::DmxStore>();
    const Result<void> registered =
        registerBuiltinNodes(*library->registry, library->snippets, library->kernels, library->dmx);
    if (!registered.ok()) {
        return registered.error();
    }
    return library;
}

}  // namespace whitehole::nodes
