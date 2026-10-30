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
    nodes/value_integration_test.cpp
    engine/frame_clocks_test.cpp
    engine/latest_frame_mailbox_test.cpp
    engine/readback_ring_test.cpp
    engine/render_engine_test.cpp
    executor/canvas_render_stage_test.cpp
    executor/canvas_shader_test.cpp
    executor/canvas_stage_graph_test.cpp
    executor/sampling_render_stage_test.cpp
    executor/sampling_shader_test.cpp
    mapping/fixture_test.cpp
    mapping/layout_test.cpp
    mapping/wiring_test.cpp
    output/artnet_test.cpp
    output/color_encoder_test.cpp
    output/ddp_test.cpp
    output/output_pipeline_test.cpp
    output/output_plan_test.cpp
    output/output_worker_test.cpp
    output/sacn_test.cpp
    output/udp_sender_test.cpp)

target_include_directories(wh_tests PRIVATE support)

target_link_libraries(wh_tests PRIVATE
    whitehole::core
    whitehole::graph
    whitehole::values
    whitehole::compiler
    whitehole::nodes
    whitehole::engine
    whitehole::executor
    whitehole::mapping
    whitehole::output
    wh_warnings
    Catch2::Catch2WithMain)

catch_discover_tests(wh_tests)
