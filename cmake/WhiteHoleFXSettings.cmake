# Gemeinsame Build-Einstellungen für alle WhiteHoleFX-Module.

# Warnungen als eigenes Interface-Target: Header-only-Module können keine
# privaten Compile-Optionen tragen, daher binden Konsumenten (Tests, Apps,
# Module mit Quelldateien) dieses Target selbst ein.
add_library(wh_warnings INTERFACE)
if(MSVC)
    target_compile_options(wh_warnings INTERFACE /W4 /permissive-)
    if(WHITEHOLEFX_WARNINGS_AS_ERRORS)
        target_compile_options(wh_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(wh_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(WHITEHOLEFX_WARNINGS_AS_ERRORS)
        target_compile_options(wh_warnings INTERFACE -Werror)
    endif()
endif()

# wh_add_library(<name> [SOURCES <dateien...>] [PUBLIC_DEPS <targets...>])
#
# Ohne SOURCES entsteht ein Header-only-Modul (INTERFACE-Target).
# Konvention: Header liegen unter <modul>/include/whitehole/<kurzname>/.
# Zusätzlich entsteht der Alias whitehole::<kurzname> (wh_core -> whitehole::core).
function(wh_add_library name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;PUBLIC_DEPS")

    if(ARG_SOURCES)
        add_library(${name} ${ARG_SOURCES})
        set(scope PUBLIC)
        target_link_libraries(${name} PRIVATE wh_warnings)
    else()
        add_library(${name} INTERFACE)
        set(scope INTERFACE)
    endif()

    string(REGEX REPLACE "^wh_" "" short_name "${name}")
    add_library(whitehole::${short_name} ALIAS ${name})

    target_include_directories(${name} ${scope} "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_compile_features(${name} ${scope} cxx_std_20)

    if(ARG_PUBLIC_DEPS)
        target_link_libraries(${name} ${scope} ${ARG_PUBLIC_DEPS})
    endif()
endfunction()
