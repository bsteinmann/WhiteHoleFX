include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

add_executable(wh_tests
    core/core_types_test.cpp
    core/hash_test.cpp
    graph/graph_test.cpp
    graph/graph_topology_test.cpp
    graph/node_registry_test.cpp
    graph/parameter_resolve_test.cpp
    graph/port_type_test.cpp
    values/cpu_kernel_test.cpp
    values/value_evaluator_test.cpp
    values/value_store_test.cpp
    compiler/field_compiler_test.cpp
    compiler/field_inputs_test.cpp
    compiler/node_definition_test.cpp
    compiler/parameter_layout_test.cpp
    compiler/snippet_expander_test.cpp
    nodes/builtin_nodes_test.cpp
    nodes/builtin_values_test.cpp
    nodes/color_values_test.cpp
    nodes/value_integration_test.cpp
    engine/frame_clocks_test.cpp
    engine/latest_frame_mailbox_test.cpp
    engine/readback_ring_test.cpp
    engine/render_engine_test.cpp
    gpu/wgsl_bindings_test.cpp
    executor/canvas_render_stage_test.cpp
    executor/canvas_shader_test.cpp
    executor/canvas_stage_graph_test.cpp
    executor/sampling_render_stage_test.cpp
    executor/sampling_shader_test.cpp
    mapping/fixture_test.cpp
    mapping/layout_test.cpp
    mapping/region_edit_test.cpp
    mapping/wiring_test.cpp
    output/artnet_test.cpp
    output/color_encoder_test.cpp
    output/ddp_test.cpp
    output/output_pipeline_test.cpp
    output/output_plan_test.cpp
    output/output_worker_test.cpp
    output/sacn_test.cpp
    output/udp_sender_test.cpp
    preview/png_test.cpp
    preview/preview_test.cpp
    preview/view_test.cpp
    project/project_test.cpp
    runtime/cli_summary_test.cpp
    runtime/graph_layout_test.cpp
    runtime/player_layout_test.cpp
    runtime/player_arrange_test.cpp
    runtime/node_clipboard_test.cpp
    runtime/node_preset_test.cpp
    runtime/player_edit_test.cpp
    runtime/player_test.cpp
    runtime/session_test.cpp
    runtime/test_pattern_test.cpp)

target_include_directories(wh_tests PRIVATE support)

target_link_libraries(wh_tests PRIVATE
    whitehole::core
    whitehole::gpu
    whitehole::graph
    whitehole::values
    whitehole::compiler
    whitehole::nodes
    whitehole::engine
    whitehole::executor
    whitehole::mapping
    whitehole::output
    whitehole::preview
    whitehole::project
    whitehole::runtime
    wh_warnings
    Catch2::Catch2WithMain)

target_include_directories(wh_tests PRIVATE "${PROJECT_SOURCE_DIR}/apps/whitehole_cli")
target_sources(wh_tests PRIVATE "${PROJECT_SOURCE_DIR}/apps/whitehole_cli/cli_options.cpp")

if(WIN32)
    target_link_libraries(wh_tests PRIVATE ws2_32)
endif()

if(WIN32)
    # Die Testnamen enthalten Umlaute; catch_discover_tests reicht sie unter Windows über die
    # Kommandozeile falsch kodiert weiter. Darum läuft das Programm dort als Ganzes.
    add_test(NAME wh_tests COMMAND wh_tests)
else()
    catch_discover_tests(wh_tests)
endif()

# Tests auf einer echten GPU (oder einem Software-Adapter): nur mit WHITEHOLEFX_WEBGPU=ON.
# Ohne Adapter werden sie übersprungen.
if(TARGET wh_gpu_webgpu)
    add_executable(wh_gpu_tests
        gpu_device/node_catalog_on_device_test.cpp
        gpu_device/node_patterns_on_device_test.cpp
        gpu_device/session_on_device_test.cpp
        gpu_device/stage_on_device_test.cpp
        gpu_device/webgpu_device_test.cpp)
    target_include_directories(wh_gpu_tests PRIVATE support)
    target_link_libraries(wh_gpu_tests PRIVATE
        wh_gpu_webgpu
        whitehole::core
        whitehole::graph
        whitehole::values
        whitehole::compiler
        whitehole::nodes
        whitehole::executor
        whitehole::mapping
        whitehole::project
        whitehole::runtime
        wh_warnings
        Catch2::Catch2WithMain)
    wh_copy_wgpu_dll(wh_gpu_tests)
    if(WIN32)
        add_test(NAME wh_gpu_tests COMMAND wh_gpu_tests)
    else()
        catch_discover_tests(wh_gpu_tests)
    endif()
endif()
