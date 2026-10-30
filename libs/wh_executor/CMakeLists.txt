# wh_executor: führt einen Graphen pro Frame auf der GPU aus.
# Verbindet die Bausteine: Werte auswerten (wh_values), Feld übersetzen (wh_compiler),
# Pipeline und Puffer über die GPU-Schnittstelle (wh_gpu) und hängt sich als
# Render-Stage in die Engine (wh_engine). Kennt weder Dawn noch Qt.
wh_add_library(wh_executor
    SOURCES
        src/canvas_render_stage.cpp
        src/canvas_shader.cpp
        src/field_pass.cpp
        src/sampling_render_stage.cpp
        src/sampling_shader.cpp
    PUBLIC_DEPS
        wh_engine
        wh_compiler
        wh_values
        wh_gpu
        wh_mapping)
