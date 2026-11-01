# wh_nodes: eingebaute Nodes. Nutzen dieselbe Schnittstelle (NodeDefinition bzw.
# CpuNodeDefinition) wie spätere Plugins; weder Compiler noch Evaluator kennen
# einen davon im Besonderen.
wh_add_library(wh_nodes
    SOURCES
        src/builtin_audio.cpp
        src/builtin_audio_field.cpp
        src/builtin_audio_wave.cpp
        src/builtin_blends.cpp
        src/builtin_generators.cpp
        src/builtin_library.cpp
        src/builtin_math.cpp
        src/builtin_sequence.cpp
        src/builtin_modifiers.cpp
        src/builtin_shapes.cpp
        src/builtin_sparks.cpp
        src/builtin_text.cpp
        src/builtin_nodes.cpp
        src/builtin_transforms.cpp
        src/builtin_volume.cpp
        src/builtin_warps.cpp
        src/builtin_color_values.cpp
        src/builtin_dmx.cpp
        src/builtin_values.cpp
    PUBLIC_DEPS
        wh_audio
        wh_compiler
        wh_input
        wh_values)
