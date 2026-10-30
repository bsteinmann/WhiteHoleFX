# wh_compiler: übersetzt Field-Nodes eines Graphen in eine einzige WGSL-Funktion.
# Hängt nur am Graph-Modell, nicht an der GPU-Schicht: Das Ergebnis ist Text
# plus Parameter-Layout, die Engine macht daraus später eine Pipeline.
wh_add_library(wh_compiler
    SOURCES
        src/field_compiler.cpp
        src/node_definition.cpp
        src/parameter_layout.cpp
        src/snippet_expander.cpp
    PUBLIC_DEPS
        wh_graph)
