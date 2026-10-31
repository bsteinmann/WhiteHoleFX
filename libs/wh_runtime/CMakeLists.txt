# wh_runtime: setzt aus einem Projekt eine laufende Ausgabe zusammen.
# Verbindet Layout, Effekt (Abtast-Stage), Engine, Ausgabe-Thread und Paketversand. Kennt die
# GPU nur über IGpuDevice; welches Backend dahintersteht, entscheidet die Anwendung. Kein Qt.
wh_add_library(wh_runtime
    SOURCES
        src/graph_layout.cpp
        src/dmx_usage.cpp
        src/led_info.cpp
        src/node_clipboard.cpp
        src/node_preset.cpp
        src/player.cpp
        src/player_edit.cpp
        src/player_layout.cpp
        src/player_arrange.cpp
        src/player_paste.cpp
        src/player_undo.cpp
        src/recent_files.cpp
        src/session.cpp
        src/summary.cpp
        src/test_pattern.cpp
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
