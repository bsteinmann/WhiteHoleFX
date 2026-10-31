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
    append(detail::sparkNodes());
    append(detail::transformNodes());
    append(detail::blendNodes());
    append(detail::modifierNodes());
    append(detail::audioFieldNodes());
    return all;
}

std::vector<values::CpuNodeDefinition> builtinCpuDefinitions(std::shared_ptr<input::DmxStore> dmx,
                                                                  std::shared_ptr<audio::AudioStore> audio) {
    std::vector<values::CpuNodeDefinition> all = detail::valueNodes();
    std::vector<values::CpuNodeDefinition> inputs = detail::dmxNodes(std::move(dmx));
    all.insert(all.end(), std::make_move_iterator(inputs.begin()), std::make_move_iterator(inputs.end()));
    std::vector<values::CpuNodeDefinition> audioInputs = detail::audioNodes(audio);
    all.insert(all.end(), std::make_move_iterator(audioInputs.begin()), std::make_move_iterator(audioInputs.end()));
    std::vector<values::CpuNodeDefinition> audioWaves = detail::audioWaveNodes(audio);
    all.insert(all.end(), std::make_move_iterator(audioWaves.begin()), std::make_move_iterator(audioWaves.end()));
    return all;
}

Result<void> registerBuiltinNodes(graph::INodeRegistrar& registrar, compiler::SnippetLibrary& snippets,
                                  values::CpuKernelLibrary& kernels, std::shared_ptr<input::DmxStore> dmx,
                                  std::shared_ptr<audio::AudioStore> audio) {
    const std::vector<compiler::NodeDefinition> fieldNodes = builtinNodeDefinitions();
    if (Result<void> registered = compiler::registerDefinitions(fieldNodes, registrar, snippets);
        !registered.ok()) {
        return registered;
    }
    const std::vector<values::CpuNodeDefinition> cpuNodes = builtinCpuDefinitions(std::move(dmx), std::move(audio));
    return values::registerCpuDefinitions(cpuNodes, registrar, kernels);
}

}  // namespace whitehole::nodes
