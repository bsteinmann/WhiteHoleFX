# whitehole_studio: Qt-Oberfläche (Qt 6, Widgets). Zeigt die Vorschau eines Projekts, startet und
# stoppt es und schaltet die Netzwerkausgabe ein. Alle Logik steckt in wh_runtime (Player); hier
# liegen nur Fenster, Menüs und das Zeichnen. Nur mit -DWHITEHOLEFX_BUILD_STUDIO=ON gebaut, weil
# Qt vorhanden sein muss.
find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

add_executable(whitehole_studio
    main.cpp
    main_window.cpp
    main_window.hpp
    preview_widget.cpp
    preview_widget.hpp)
target_link_libraries(whitehole_studio PRIVATE
    Qt6::Widgets
    whitehole::nodes
    whitehole::output
    whitehole::preview
    whitehole::project
    whitehole::runtime
    wh_warnings)
# Qt definiert "slots", "signals" und "emit" als Makros; unser Code benutzt "slots" als Namen
# (OutputPlan::slots). QT_NO_KEYWORDS schaltet die Makros ab, Qt-Code nutzt Q_SIGNALS/Q_EMIT.
target_compile_definitions(whitehole_studio PRIVATE QT_NO_KEYWORDS)
if(TARGET wh_gpu_webgpu)
    target_link_libraries(whitehole_studio PRIVATE wh_gpu_webgpu)
    target_compile_definitions(whitehole_studio PRIVATE WH_STUDIO_HAVE_GPU=1)
    wh_copy_wgpu_dll(whitehole_studio)
endif()
if(WIN32)
    set_target_properties(whitehole_studio PROPERTIES WIN32_EXECUTABLE ON)
endif()
