# wh_mapping: wo sitzt welche LED im Raum?
# Reine Rechenlogik ohne GPU und ohne Qt: Fixtures (Matrix, Strip, 3D-Gitter,
# Punktwolke) werden zu einer LedMap gebacken, die jeder LED eine Position im
# Einheitswürfel [0,1]^3 gibt. Der Abtast-Pass (Teil 5b) liest diese Karte.
wh_add_library(wh_mapping
    SOURCES
        src/fixture.cpp
        src/layout.cpp
        src/mini_json.cpp
        src/point_import.cpp
        src/region_edit.cpp
        src/wiring.cpp
    PUBLIC_DEPS
        wh_core)
