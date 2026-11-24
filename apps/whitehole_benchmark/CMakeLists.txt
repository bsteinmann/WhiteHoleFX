# whitehole_benchmark: der Benchmark als eigenes Konsolenprogramm, ohne Qt und ohne Projekt (Doppelklick genügt).
# Die Messung selbst steckt in wh_benchmark_run (dieselbe wie bei `whitehole_cli --benchmark`). Ohne GPU-Backend
# wäre das Programm nutzlos und wird deshalb gar nicht erst gebaut (siehe Wurzel-CMakeLists).
add_executable(whitehole_benchmark main.cpp benchmark_args.cpp)
target_compile_definitions(whitehole_benchmark PRIVATE WH_VERSION="${PROJECT_VERSION}" WH_BUILD_ID="${WH_BUILD_ID}")
target_link_libraries(whitehole_benchmark PRIVATE
    whitehole::nodes
    whitehole::runtime
    wh_benchmark_run
    wh_gpu_webgpu
    wh_warnings)
wh_copy_wgpu_dll(whitehole_benchmark)
