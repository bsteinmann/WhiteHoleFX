# wh_output: vom Farbpuffer zum Netzwerkpaket.
# Farbumrechnung (Gamma, Helligkeit, Farbreihenfolge), Verteilung der LEDs auf Universen,
# die Paketformate von Art-Net, sACN (E1.31) und DDP, die Pipeline pro Frame, ein
# Ausgabe-Thread und der UDP-Versand. Nur udp_sender.cpp berührt das Betriebssystem; alles
# andere ist ohne Netzwerk testbar. Kein Qt.
find_package(Threads REQUIRED)

wh_add_library(wh_output
    SOURCES
        src/artnet.cpp
        src/color_encoder.cpp
        src/ddp.cpp
        src/output_pipeline.cpp
        src/output_plan.cpp
        src/output_worker.cpp
        src/sacn.cpp
        src/udp_sender.cpp
    PUBLIC_DEPS
        wh_core
        wh_engine
        wh_mapping
        Threads::Threads)
