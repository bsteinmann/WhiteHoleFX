# wh_gpu: GPU-Abstraktion (Handles, Beschreibungen, IGpuDevice) und der Leser für die
# Ressourcen-Bindungen eines WGSL-Shaders. Das WebGPU-Backend (Dawn/wgpu-native) ist ein
# eigenes, optionales Modul und hängt an dieser Schnittstelle, nicht umgekehrt.
wh_add_library(wh_gpu
    SOURCES
        src/wgsl_bindings.cpp
    PUBLIC_DEPS
        wh_core)
