cmake_minimum_required(VERSION 3.25)

project(WhiteHoleFX
    VERSION 0.1.0
    DESCRIPTION "Open-Source-Echtzeitsteuerung für LED-Matrizen, LED-Cubes und Pixelinstallationen"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WHITEHOLEFX_BUILD_TESTS "Unit-Tests bauen" ON)
option(WHITEHOLEFX_WEBGPU "WebGPU-Backend bauen (braucht wgpu-native, siehe docs/gpu-backend.md)" OFF)
option(WHITEHOLEFX_WARNINGS_AS_ERRORS "Compiler-Warnungen als Fehler behandeln" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(WhiteHoleFXSettings)

# Module in Abhängigkeitsreihenfolge:
#   core <- gpu <- engine
#   core <- graph <- compiler <- nodes
#   core <- graph <- values   <- nodes
#   engine, values, compiler, gpu <- executor
#   core <- mapping
#   gpu <- gpu_webgpu (optional, WHITEHOLEFX_WEBGPU)
add_subdirectory(libs/wh_core)
add_subdirectory(libs/wh_gpu)
add_subdirectory(libs/wh_graph)
add_subdirectory(libs/wh_values)
add_subdirectory(libs/wh_compiler)
add_subdirectory(libs/wh_nodes)
add_subdirectory(libs/wh_engine)
add_subdirectory(libs/wh_mapping)
add_subdirectory(libs/wh_output)
add_subdirectory(libs/wh_executor)
add_subdirectory(libs/wh_project)
add_subdirectory(libs/wh_preview)
add_subdirectory(libs/wh_runtime)
if(WHITEHOLEFX_WEBGPU)
    add_subdirectory(libs/wh_gpu_webgpu)
endif()

option(WHITEHOLEFX_BUILD_APPS "Konsolen-Runner bauen" ON)
if(WHITEHOLEFX_BUILD_APPS)
    add_subdirectory(apps/whitehole_cli)
endif()

if(WHITEHOLEFX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
