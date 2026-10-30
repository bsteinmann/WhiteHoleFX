# wh_runtime: setzt aus einem Projekt eine laufende Ausgabe zusammen.
# Verbindet Layout, Effekt (Abtast-Stage), Engine, Ausgabe-Thread und Paketversand. Kennt die
# GPU nur über IGpuDevice; welches Backend dahintersteht, entscheidet die Anwendung. Kein Qt.
wh_add_library(wh_runtime
    SOURCES
        src/player.cpp
        src/session.cpp
        src/summary.cpp
    PUBLIC_DEPS
        wh_core
        wh_engine
        wh_executor
        wh_gpu
        wh_mapping
        wh_nodes
        wh_output
        wh_preview
        wh_project)
