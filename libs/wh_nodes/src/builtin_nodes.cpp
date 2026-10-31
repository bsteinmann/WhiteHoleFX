// builtin_nodes.cpp: Umsetzung zu builtin_nodes.hpp. Die eingebauten Nodes (Field-Nodes und Werte-Nodes).
#include "whitehole/nodes/builtin_nodes.hpp"

#include <iterator>
#include <utility>

#include "builtin_internal.hpp"

namespace whitehole::nodes {

std::vector<compiler::NodeDefinition> builtinNodeDefinitions() {
    std::vector<compiler::NodeDefinition> all;
    const auto append = [&all](std::vector<compiler::NodeDefinition> group) {
        all.insert(all.end(), std::make_move_iterator(group.begin()),
                   std::make_move_iterator(group.end()));
    };
    append(detail::generatorNodes());
    append(detail::shapeNodes());
    append(detail::transformNodes());
    append(detail::blendNodes());
    append(detail::modifierNodes());
    return all;
}

std::vector<values::CpuNodeDefinition> builtinCpuDefinitions(std::shared_ptr<input::DmxStore> dmx) {
    std::vector<values::CpuNodeDefinition> all = detail::valueNodes();
    std::vector<values::CpuNodeDefinition> inputs = detail::dmxNodes(std::move(dmx));
    all.insert(all.end(), std::make_move_iterator(inputs.begin()), std::make_move_iterator(inputs.end()));
    return all;
}

Result<void> registerBuiltinNodes(graph::INodeRegistrar& registrar, compiler::SnippetLibrary& snippets,
                                  values::CpuKernelLibrary& kernels, std::shared_ptr<input::DmxStore> dmx) {
    const std::vector<compiler::NodeDefinition> fieldNodes = builtinNodeDefinitions();
    if (Result<void> registered = compiler::registerDefinitions(fieldNodes, registrar, snippets);
        !registered.ok()) {
        return registered;
    }
    const std::vector<values::CpuNodeDefinition> cpuNodes = builtinCpuDefinitions(std::move(dmx));
    return values::registerCpuDefinitions(cpuNodes, registrar, kernels);
}

}  // namespace whitehole::nodes
