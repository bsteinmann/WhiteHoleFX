# wh_preview: LED-Vorschau als Bild (Rasterisieren der LED-Karte) und ein PNG-Schreiber.
# Reine CPU-Rechnung ohne Fremdbibliothek, kein Qt. Die spätere Oberfläche zeigt dasselbe Bild
# in einem Fenster; der Konsolen-Runner schreibt es als Datei.
wh_add_library(wh_preview
    SOURCES
        src/png.cpp
        src/preview.cpp
        src/view.cpp
    PUBLIC_DEPS
        wh_core
        wh_mapping)
