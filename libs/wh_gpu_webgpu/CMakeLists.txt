# wh_gpu_webgpu: IGpuDevice auf Basis der WebGPU-C-Schnittstelle (webgpu.h).
#
# Optional (WHITEHOLEFX_WEBGPU=ON), weil dafür eine WebGPU-Bibliothek vorhanden sein muss.
# Getestet wird gegen wgpu-native; die Schnittstelle webgpu.h ist dieselbe wie bei Dawn,
# aber die Header-Version muss zur Bibliothek passen (siehe docs/gpu-backend.md).
#
# Suche: WGPU_NATIVE_ROOT (oder CMAKE_PREFIX_PATH) mit include/webgpu/webgpu.h,
# include/webgpu/wgpu.h und lib/libwgpu_native.{so,a,dylib}.
find_path(WGPU_NATIVE_INCLUDE_DIR webgpu/webgpu.h
    HINTS "${WGPU_NATIVE_ROOT}/include" ENV WGPU_NATIVE_ROOT)
# Windows: das Paket enthält wgpu_native.dll samt Importbibliothek wgpu_native.dll.lib.
# Mit der Importbibliothek entfallen die Systembibliotheken der statischen Rust-Variante.
find_library(WGPU_NATIVE_LIBRARY NAMES wgpu_native.dll wgpu_native
    HINTS "${WGPU_NATIVE_ROOT}/lib" ENV WGPU_NATIVE_ROOT)
if(WIN32)
    find_file(WGPU_NATIVE_DLL wgpu_native.dll
        HINTS "${WGPU_NATIVE_ROOT}/lib" ENV WGPU_NATIVE_ROOT)
endif()

if(NOT WGPU_NATIVE_INCLUDE_DIR OR NOT WGPU_NATIVE_LIBRARY)
    message(FATAL_ERROR
        "WHITEHOLEFX_WEBGPU=ON, aber wgpu-native wurde nicht gefunden. "
        "Mit -DWGPU_NATIVE_ROOT=<Ordner mit include/ und lib/> angeben.")
endif()

find_package(Threads REQUIRED)

wh_add_library(wh_gpu_webgpu
    SOURCES
        src/webgpu_commands.cpp
        src/webgpu_convert.cpp
        src/webgpu_device.cpp
        src/webgpu_resources.cpp
    PUBLIC_DEPS
        wh_core
        wh_gpu)

# Die WebGPU-Header bleiben privat: Kein Header dieses Moduls enthält webgpu.h.
target_include_directories(wh_gpu_webgpu PRIVATE "${WGPU_NATIVE_INCLUDE_DIR}" src)
target_link_libraries(wh_gpu_webgpu PRIVATE "${WGPU_NATIVE_LIBRARY}" Threads::Threads ${CMAKE_DL_LIBS})
target_compile_definitions(wh_gpu_webgpu PRIVATE WH_WEBGPU_WGPU_NATIVE=1)

# Windows: Die DLL muss neben den ausführbaren Dateien liegen. Wer das Modul einbindet,
# ruft wh_copy_wgpu_dll(<ziel>) auf.
function(wh_copy_wgpu_dll target)
    if(WIN32 AND WGPU_NATIVE_DLL)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WGPU_NATIVE_DLL}" "$<TARGET_FILE_DIR:${target}>")
    endif()
endfunction()
