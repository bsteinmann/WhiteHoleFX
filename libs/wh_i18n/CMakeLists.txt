# wh_i18n: Übersetzungskataloge für die Oberfläche (ohne Qt, ohne GPU).
# Liest Kataloge im gettext-Format (.po), damit Übersetzer ein gewöhnliches Werkzeug (Poedit, Weblate)
# nutzen können, und prüft sie (Platzhalter, Vollständigkeit). Die Qt-Oberfläche hängt nur einen dünnen
# QTranslator davor; so lässt sich die ganze Katalogarbeit ohne Qt testen.
wh_add_library(wh_i18n
    SOURCES
        src/language.cpp
        src/po_catalog.cpp
    PUBLIC_DEPS
        wh_core)
