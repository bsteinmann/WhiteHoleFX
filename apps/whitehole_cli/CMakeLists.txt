# whitehole_cli: Konsolen-Runner. Liest eine Projektdatei, prüft sie (--check) oder rendert
# sie und sendet Art-Net, sACN oder DDP. Das Rendern braucht das GPU-Backend
# (-DWHITEHOLEFX_WEBGPU=ON); ohne es läuft nur --check.
add_executable(whitehole_cli main.cpp cli_options.cpp)
target_link_libraries(whitehole_cli PRIVATE
    whitehole::audio
    whitehole::input
    whitehole::nodes
    whitehole::preview
    whitehole::project
    whitehole::runtime
    wh_warnings)
if(TARGET wh_gpu_webgpu)
    target_link_libraries(whitehole_cli PRIVATE wh_gpu_webgpu)
    target_compile_definitions(whitehole_cli PRIVATE WH_CLI_HAVE_GPU=1)
    wh_copy_wgpu_dll(whitehole_cli)
endif()
