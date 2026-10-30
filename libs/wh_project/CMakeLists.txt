# wh_project: Projektdatei (Layout, Effektgraph, Ausgabe) lesen und schreiben.
# Reine Textverarbeitung, kein Netzwerk und keine GPU. Kein Qt.
wh_add_library(wh_project
    SOURCES
        src/project_fixture_parser.cpp
        src/project_parser.cpp
        src/project_text.cpp
        src/project_writer.cpp
    PUBLIC_DEPS
        wh_core
        wh_graph
        wh_mapping
        wh_output)
target_include_directories(wh_project PRIVATE src)
