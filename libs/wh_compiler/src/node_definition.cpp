#include "whitehole/compiler/node_definition.hpp"

#include <utility>

namespace whitehole::compiler {

const NodeSnippet* SnippetLibrary::find(std::string_view typeId) const {
    const auto it = snippets_.find(typeId);
    return it == snippets_.end() ? nullptr : &it->second;
}

Result<void> SnippetLibrary::add(std::string typeId, NodeSnippet snippet) {
    if (snippets_.contains(typeId)) {
        return Error{ErrorCode::AlreadyExists,
                     "Für '" + typeId + "' gibt es bereits ein Shader-Snippet"};
    }
    snippets_.emplace(std::move(typeId), std::move(snippet));
    return {};
}

Result<void> registerDefinitions(std::span<const NodeDefinition> definitions,
                                 graph::INodeRegistrar& registrar, SnippetLibrary& library) {
    for (const NodeDefinition& definition : definitions) {
        if (Result<void> registered = registrar.registerNode(definition.descriptor); !registered.ok()) {
            return registered;
        }
        if (Result<void> added = library.add(definition.descriptor.typeId, definition.snippet);
            !added.ok()) {
            return added;
        }
    }
    return {};
}

}  // namespace whitehole::compiler
