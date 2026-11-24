# wh_benchmark_run: der Teil des Benchmarks, den `whitehole_cli --benchmark` und `whitehole_benchmark` gemeinsam
# brauchen (Lauf auf der GPU, Berichtsdateien). Der Messkern liegt in wh_runtime; hier steht nur, was Konsolen-
# Programme darum herum tun. Das GPU-Backend ist optional wie bei whitehole_cli.
add_library(wh_benchmark_run STATIC benchmark_files.cpp benchmark_job.cpp)
target_include_directories(wh_benchmark_run PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(wh_benchmark_run
    PUBLIC
        whitehole::core
        whitehole::nodes
        whitehole::runtime
    PRIVATE
        wh_warnings)
if(TARGET wh_gpu_webgpu)
    target_link_libraries(wh_benchmark_run PRIVATE wh_gpu_webgpu)
    target_compile_definitions(wh_benchmark_run PRIVATE WH_BENCH_HAVE_GPU=1)
endif()
