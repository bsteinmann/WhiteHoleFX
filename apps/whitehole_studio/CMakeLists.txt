# whitehole_studio: Qt-Oberfläche (Qt 6, Widgets). Zeigt die Vorschau eines Projekts, startet und
# stoppt es und schaltet die Netzwerkausgabe ein. Alle Logik steckt in wh_runtime (Player); hier
# liegen nur Fenster, Menüs und das Zeichnen. Nur mit -DWHITEHOLEFX_BUILD_STUDIO=ON gebaut, weil
# Qt vorhanden sein muss.
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)  # studio.qrc: Übersetzungskataloge, Tutorial und Programmsymbol
if(WIN32)
    enable_language(RC)  # für whitehole_studio.rc (Programmsymbol der .exe)
endif()

add_executable(whitehole_studio
    fixture_form.cpp
    fixture_form_exercise.cpp
    fixture_form.hpp
    form_helpers.hpp
    layout_panel.cpp
    scroll_behavior.cpp
    layout_panel_add.cpp
    output_page.cpp
    output_page.hpp
    patch_table.cpp
    patch_table.hpp
    layout_panel_types.cpp
    layout_panel_grid.cpp
    layout_panel_library.cpp
    layout_panel_groups.cpp
    layout_panel.hpp
    main.cpp
    main_window.cpp
    main_window_actions.cpp
    main_window_audio.cpp
    main_window_controls.cpp
    main_window_cues.cpp
    main_window_dmx.cpp
    main_window_editor.cpp
    main_window_live.cpp
    main_window_help.cpp
    main_window_update.cpp
    update_checker.cpp
    update_checker.hpp
    main_window_language.cpp
    main_window_layout.cpp
    app_language.cpp
    app_language.hpp
    studio.qrc
    stall_event_filter.hpp
    stall_watchdog.cpp
    stall_watchdog_win.cpp
    crash_notice.cpp
    crash_notice.hpp
    stall_watchdog.hpp
    main_window_settings.cpp
    main_window_stage.cpp
    main_window_view3d.cpp
    cue_dialog.cpp
    cue_dialog.hpp
    group_dialog.cpp
    group_dialog.hpp
    cue_panel.cpp
    cue_panel.hpp
    live_panel.cpp
    live_panel.hpp
    look_panel.cpp
    look_panel.hpp
    main_window_exercise.cpp
    main_window_file.cpp
    main_window.hpp
    node_editor.cpp
    node_editor_clipboard.cpp
    node_editor_dock.cpp
    node_editor_drag.cpp
    node_editor_edit.cpp
    node_editor_exercise.cpp
    node_editor.hpp
    node_editor_connect.cpp
    node_editor_menu.cpp
    node_editor_targets.cpp
    port_legend.cpp
    node_editor_presets.cpp
    node_editor_search.cpp
    node_info.cpp
    node_info.hpp
    node_info_texts.cpp
    node_items.cpp
    node_items_group.cpp
    node_items_live.cpp
    node_editor_live.cpp
    node_editor_live_exercise.cpp
    node_editor_group.cpp
    node_editor_group_dialog.cpp
    node_editor_library.cpp
    node_palette_library.cpp
    node_editor_group_exercise.cpp
    node_palette.cpp
    node_palette.hpp
    node_search.hpp
    node_items.hpp
    output_form.cpp
    output_form_library.cpp
    output_form.hpp
    parameter_editors.cpp
    parameter_editors.hpp
    parameter_panel.cpp
    parameter_panel.hpp
    preview_controller.cpp
    preview_controller_exercise.cpp
    preview_controller.hpp
    add_fixture_dialog.cpp
    add_fixture_dialog.hpp
    app_settings.cpp
    app_settings.hpp
    network_interfaces.cpp
    node_defaults_page.cpp
    node_defaults_page.hpp
    network_interfaces.hpp
    settings_dialog.cpp
    settings_dialog.hpp
    preview_widget.cpp
    preview_widget.hpp
    fixture_library_exercise.cpp
    fixture_library_page.cpp
    fixture_library_user.cpp
    user_controller_library.cpp
    user_library.cpp
    user_library.hpp
    fixture_library_page.hpp
    shape_form.cpp
    shape_form.hpp
    shape_preview.cpp
    shape_preview.hpp
    stage_setup_window.cpp
    stage_view.cpp
    stage_view_gizmo.cpp
    stage_view_rings.cpp
    splash.cpp
    splash.hpp
    stage_view.hpp
    stage_setup_window.hpp
    test_panel.cpp
    test_panel.hpp)
target_link_libraries(whitehole_studio PRIVATE
    Qt6::Widgets
    Qt6::Network
    whitehole::audio
    whitehole::i18n
    whitehole::input
    whitehole::nodes
    whitehole::output
    whitehole::preview
    whitehole::project
    whitehole::runtime
    wh_warnings)
# Qt definiert "slots", "signals" und "emit" als Makros; unser Code benutzt "slots" als Namen
# (OutputPlan::slots). QT_NO_KEYWORDS schaltet die Makros ab, Qt-Code nutzt Q_SIGNALS/Q_EMIT.
target_compile_definitions(whitehole_studio PRIVATE QT_NO_KEYWORDS WH_VERSION="${PROJECT_VERSION}")
if(TARGET wh_gpu_webgpu)
    target_link_libraries(whitehole_studio PRIVATE wh_gpu_webgpu)
    target_compile_definitions(whitehole_studio PRIVATE WH_STUDIO_HAVE_GPU=1)
    wh_copy_wgpu_dll(whitehole_studio)
endif()
if(UNIX AND NOT APPLE)
    # -rdynamic: Der Wächter (stall_watchdog.cpp) gibt bei Freeze oder Absturz den Aufrufstapel aus; mit
    # exportierten Symbolen stehen dort Funktionsnamen statt nur Adressen.
    set_target_properties(whitehole_studio PROPERTIES ENABLE_EXPORTS ON)
endif()
if(WIN32)
    set_target_properties(whitehole_studio PROPERTIES WIN32_EXECUTABLE ON)
    # Programmsymbol der .exe (whitehole_studio.rc bindet packaging/windows/whiteholefx.ico ein).
    target_sources(whitehole_studio PRIVATE whitehole_studio.rc)
    # Absturzbericht (stall_watchdog_win.cpp): Minidump und Aufrufstapel über dbghelp.
    target_link_libraries(whitehole_studio PRIVATE dbghelp)
endif()
if(MSVC)
    # Auch im Release eine .pdb erzeugen: Mit ihr lassen sich Adressen aus einem Absturzbericht in Funktionsnamen
    # und Zeilen auflösen. Die .pdb gehört nicht ins Paket; die CI legt sie als eigenes Artefakt ab.
    target_compile_options(whitehole_studio PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(whitehole_studio PRIVATE $<$<CONFIG:Release>:/DEBUG> $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
endif()
