# WhiteHoleFXBuildId.cmake: setzt WH_BUILD_ID für Studio und CLI.
#
# Warum es die Datei gibt: Build-Kennung für Absturz- und Stillstandsberichte (ABSTUERZE_TODO.md Punkt 6): Die Versionsnummer allein unterscheidet
# den CI-Lauf nicht vom Release-Lauf desselben Stands, und nur der Commit und die Lauf-Nummer führen zur passenden .pdb.
# Der Commit wird beim Konfigurieren gelesen ("unbekannt" ohne Git, etwa aus einem git-archive-ZIP); in der CI kommt die
# Lauf-Nummer aus GITHUB_RUN_ID dazu. Bewusst ohne Leerzeichen und Kommas, weil der Wert als Compiler-Definition durchgereicht
# wird (Beispiel: commit-5a6da9d507_lauf-44056022233).
set(WH_BUILD_ID "unbekannt")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=10 HEAD
                    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                    OUTPUT_VARIABLE WH_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE WH_GIT_RESULT ERROR_QUIET)
    if(WH_GIT_RESULT EQUAL 0 AND NOT WH_GIT_HASH STREQUAL "")
        set(WH_BUILD_ID "commit-${WH_GIT_HASH}")
    endif()
endif()
if(DEFINED ENV{GITHUB_RUN_ID})
    set(WH_BUILD_ID "${WH_BUILD_ID}_lauf-$ENV{GITHUB_RUN_ID}")
endif()
