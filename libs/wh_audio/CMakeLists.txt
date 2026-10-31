# wh_audio: Audio-Analyse (Pegel, 32-Band-Spektrum, Beat/Tempo) und Audio-Eingang.
# Der Kern (Fft, AudioAnalyzer, BeatDetector, AudioStore, WAV) rechnet nur auf Samples und ist ohne
# Gerät testbar. Nur capture_device.cpp berührt das Betriebssystem, über miniaudio (wird beim
# Konfigurieren geladen). Mit -DWHITEHOLEFX_AUDIO_CAPTURE=OFF entfällt das, WAV-Quellen bleiben.
# Kein Qt.
find_package(Threads REQUIRED)

option(WHITEHOLEFX_AUDIO_CAPTURE "Audio-Aufnahme von Geräten (miniaudio, wird geladen)" ON)

set(WH_AUDIO_SOURCES
    src/analyzer.cpp
    src/audio_input.cpp
    src/audio_store.cpp
    src/beat_detector.cpp
    src/fft.cpp
    src/wav.cpp)

if(WHITEHOLEFX_AUDIO_CAPTURE)
    include(FetchContent)
    # SOURCE_SUBDIR auf einen Ordner ohne CMakeLists: Wir wollen nur den Header, nicht das Projekt.
    FetchContent_Declare(miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG        0.11.22
        SOURCE_SUBDIR  wh-nur-header)
    FetchContent_MakeAvailable(miniaudio)

    # Die Bibliothek in einer eigenen Übersetzungseinheit und ohne unsere Warnoptionen.
    add_library(wh_miniaudio STATIC src/miniaudio_impl.cpp)
    target_include_directories(wh_miniaudio SYSTEM PUBLIC "${miniaudio_SOURCE_DIR}")
    target_link_libraries(wh_miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
    if(UNIX AND NOT APPLE)
        target_link_libraries(wh_miniaudio PUBLIC m)
    endif()
    set_target_properties(wh_miniaudio PROPERTIES POSITION_INDEPENDENT_CODE ON)
    list(APPEND WH_AUDIO_SOURCES src/capture_device.cpp)
else()
    list(APPEND WH_AUDIO_SOURCES src/capture_device_stub.cpp)
endif()

wh_add_library(wh_audio
    SOURCES ${WH_AUDIO_SOURCES}
    PUBLIC_DEPS
        wh_core
        Threads::Threads)

if(WHITEHOLEFX_AUDIO_CAPTURE)
    target_link_libraries(wh_audio PRIVATE wh_miniaudio)
    target_compile_definitions(wh_audio PUBLIC WHITEHOLEFX_HAS_AUDIO_CAPTURE=1)
endif()
