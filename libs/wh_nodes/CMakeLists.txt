# wh_nodes: eingebaute Nodes. Nutzen dieselbe Schnittstelle (NodeDefinition bzw.
# CpuNodeDefinition) wie spätere Plugins; weder Compiler noch Evaluator kennen
# einen davon im Besonderen.
wh_add_library(wh_nodes
    SOURCES
        src/builtin_blends.cpp
        src/builtin_generators.cpp
        src/builtin_library.cpp
        src/builtin_nodes.cpp
        src/builtin_transforms.cpp
        src/builtin_values.cpp
    PUBLIC_DEPS
        wh_compiler
        wh_values)
