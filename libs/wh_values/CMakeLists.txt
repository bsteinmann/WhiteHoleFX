# wh_values: Werte-Graph. Wertet die CPU-Nodes (Zeit, LFO, Zufall, später Audio und
# Home Assistant) einmal pro Frame aus und legt die Ergebnisse in einem ValueStore ab.
# Hängt nur am Graph-Modell: Weder GPU noch Compiler sind nötig, der Compiler
# liest die Ergebnisse später über die Schnittstelle graph::IValueLookup.
wh_add_library(wh_values
    SOURCES
        src/cpu_kernel.cpp
        src/value_evaluator.cpp
        src/value_store.cpp
    PUBLIC_DEPS
        wh_graph)
