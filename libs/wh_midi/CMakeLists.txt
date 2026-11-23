# wh_midi: MIDI-Eingang und -Ausgang (Noten, Regler, LED-Rückmeldung an Controller wie den Akai APC mini).
# Der Kern (MidiMessage, MidiStore, MidiReceiver, Portsuche) rechnet nur auf Nachrichten und ist ohne Gerät
# testbar. Nur rtmidi_backend.cpp berührt das Betriebssystem, über RtMidi (ALSA unter Linux, WinMM unter
# Windows; wird beim Konfigurieren geladen). Fehlen unter Linux die ALSA-Entwicklungsdateien (libasound2-dev) oder
# ist -DWHITEHOLEFX_MIDI_DEVICES=OFF gesetzt, entfällt die Geräteunterstützung, und ein Stub meldet keine Ports.
# Kein Qt.
find_package(Threads REQUIRED)

option(WHITEHOLEFX_MIDI_DEVICES "MIDI-Geräte ansprechen (RtMidi, wird geladen)" ON)

set(WH_MIDI_SOURCES
    src/midi_message.cpp
    src/midi_port_search.cpp
    src/midi_receiver.cpp
    src/midi_store.cpp)

set(wh_midi_devices OFF)
if(WHITEHOLEFX_MIDI_DEVICES)
    if(WIN32)
        set(wh_midi_devices ON)
    elseif(UNIX AND NOT APPLE)
        find_path(WH_ALSA_INCLUDE_DIR alsa/asoundlib.h)
        find_library(WH_ALSA_LIBRARY asound)
        if(WH_ALSA_INCLUDE_DIR AND WH_ALSA_LIBRARY)
            set(wh_midi_devices ON)
        else()
            message(WARNING "wh_midi: ALSA-Entwicklungsdateien fehlen (libasound2-dev) - MIDI ohne Geräteunterstützung")
        endif()
    endif()
endif()

if(wh_midi_devices)
    include(FetchContent)
    # SOURCE_SUBDIR auf einen Ordner ohne CMakeLists: Wir wollen nur Quelltext und Header, nicht das Projekt.
    FetchContent_Declare(rtmidi
        GIT_REPOSITORY https://github.com/thestk/rtmidi.git
        GIT_TAG        6.0.0
        SOURCE_SUBDIR  wh-nur-quelltext)
    FetchContent_MakeAvailable(rtmidi)

    add_library(wh_rtmidi STATIC src/rtmidi_impl.cpp)
    target_include_directories(wh_rtmidi SYSTEM PUBLIC "${rtmidi_SOURCE_DIR}")
    target_include_directories(wh_rtmidi PRIVATE "${rtmidi_SOURCE_DIR}")
    target_link_libraries(wh_rtmidi PUBLIC Threads::Threads)
    if(WIN32)
        target_compile_definitions(wh_rtmidi PUBLIC __WINDOWS_MM__)
        target_link_libraries(wh_rtmidi PUBLIC winmm)
    else()
        target_compile_definitions(wh_rtmidi PUBLIC __LINUX_ALSA__)
        target_include_directories(wh_rtmidi SYSTEM PUBLIC "${WH_ALSA_INCLUDE_DIR}")
        target_link_libraries(wh_rtmidi PUBLIC "${WH_ALSA_LIBRARY}")
    endif()
    set_target_properties(wh_rtmidi PROPERTIES POSITION_INDEPENDENT_CODE ON)
    list(APPEND WH_MIDI_SOURCES src/rtmidi_backend.cpp)
else()
    list(APPEND WH_MIDI_SOURCES src/midi_backend_stub.cpp)
endif()

wh_add_library(wh_midi
    SOURCES ${WH_MIDI_SOURCES}
    PUBLIC_DEPS
        wh_core
        Threads::Threads)

if(wh_midi_devices)
    target_link_libraries(wh_midi PRIVATE wh_rtmidi)
endif()
