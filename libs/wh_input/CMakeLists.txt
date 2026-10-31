# wh_input: DMX-Eingang aus dem Netzwerk.
# Liest Art-Net (ArtDmx) und sACN (E1.31), hält den letzten Zustand jedes Universe in einem
# DmxStore und stellt den UDP-Empfänger. Nur udp_listener.cpp berührt das Betriebssystem; die
# Paketauswertung und der Speicher sind ohne Netzwerk testbar. Kein Qt.
find_package(Threads REQUIRED)

wh_add_library(wh_input
    SOURCES
        src/dmx_packets.cpp
        src/dmx_receiver.cpp
        src/dmx_store.cpp
        src/udp_listener.cpp
    PUBLIC_DEPS
        wh_core
        Threads::Threads)

if(WIN32)
    target_link_libraries(wh_input PUBLIC ws2_32)
endif()
