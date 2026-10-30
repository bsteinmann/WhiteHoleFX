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
    append(detail::transformNodes());
    append(detail::blendNodes());
    return all;
}

std::vector<values::CpuNodeDefinition> builtinCpuDefinitions() { return detail::valueNodes(); }

Result<void> registerBuiltinNodes(graph::INodeRegistrar& registrar, compiler::SnippetLibrary& snippets,
                                  values::CpuKernelLibrary& kernels) {
    const std::vector<compiler::NodeDefinition> fieldNodes = builtinNodeDefinitions();
    if (Result<void> registered = compiler::registerDefinitions(fieldNodes, registrar, snippets);
        !registered.ok()) {
        return registered;
    }
    const std::vector<values::CpuNodeDefinition> cpuNodes = builtinCpuDefinitions();
    return values::registerCpuDefinitions(cpuNodes, registrar, kernels);
}

}  // namespace whitehole::nodes
