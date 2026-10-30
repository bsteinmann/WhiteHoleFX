# wh_engine: Rahmen des Renderloops (Takt, Einreichen, asynchrones Readback).
# Hängt nur an Schnittstellen (wh_core, wh_gpu), nicht an Qt oder Dawn.
find_package(Threads REQUIRED)

wh_add_library(wh_engine
    SOURCES
        src/latest_frame_mailbox.cpp
        src/readback_ring.cpp
        src/render_engine.cpp
    PUBLIC_DEPS
        wh_core
        wh_gpu
        Threads::Threads)
