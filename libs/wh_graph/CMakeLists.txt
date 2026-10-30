# wh_graph: Graph-Modell (Nodes, Ports, Verbindungen, Parameter), Registry, Topologie.
# Bewusst ohne GPU-Abhängigkeit: Der Graph beschreibt nur Struktur.
# Die Übersetzung in Shader übernimmt wh_compiler, die Ausführung wh_engine.
wh_add_library(wh_graph
    SOURCES
        src/graph.cpp
        src/graph_topology.cpp
        src/node_registry_impl.cpp
        src/parameter_resolve.cpp
    PUBLIC_DEPS
        wh_core)
