# Installation und Linux-Paket (.deb).
#
# Warum es diese Datei gibt: "cmake --install" und "cpack" sollen ein vollständiges, auf anderen
# Rechnern installierbares Programm liefern. Dazu gehören neben den Programmen auch die Beispiele,
# die Dokumentation, Starter, Symbol und Dateityp, sowie wgpu-native, falls es als Bibliothek
# (.so) gelinkt wurde. Das Paket entsteht nur, wenn Oberfläche und GPU-Backend gebaut werden.
#
# Aufruf (nach dem Bauen mit Release und -DCMAKE_INSTALL_PREFIX=/usr):
#   cpack --config build/CPackConfig.cmake -G DEB
include_guard(GLOBAL)
include(GNUInstallDirs)
# Der Ordner unter share/doc muss wie das Debian-Paket heißen (klein geschrieben).

install(TARGETS whitehole_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(TARGET whitehole_studio)
    install(TARGETS whitehole_studio RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(DIRECTORY "${PROJECT_SOURCE_DIR}/examples/"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/whiteholefx/examples
    FILES_MATCHING PATTERN "*.whfx" PATTERN "*.csv")

file(GLOB WH_DOC_FILES "${PROJECT_SOURCE_DIR}/docs/*.md")
install(FILES "${PROJECT_SOURCE_DIR}/README.md" "${PROJECT_SOURCE_DIR}/BUILDING.md"
              "${PROJECT_SOURCE_DIR}/CHANGELOG.md" "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/NOTICE" ${WH_DOC_FILES}
    DESTINATION share/doc/whiteholefx)
install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/copyright" DESTINATION share/doc/whiteholefx)

if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/whitehole-studio.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/whiteholefx.svg"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/whiteholefx-mime.xml"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
endif()

# wgpu-native als Bibliothek (.so): neben den Programmen unter lib/whiteholefx ablegen. Die
# Programme finden sie über ihren RPATH ($ORIGIN/../lib/whiteholefx, siehe oberste CMakeLists).
if(WGPU_NATIVE_LIBRARY MATCHES "\\.so(\\.[0-9]+)*$")
    install(FILES "${WGPU_NATIVE_LIBRARY}" DESTINATION ${CMAKE_INSTALL_LIBDIR}/whiteholefx)
endif()

# --- CPack: .deb ------------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "whiteholefx")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "bsteinmann <bsteinmann@users.noreply.github.com>")
set(CPACK_PACKAGE_VENDOR "WhiteHoleFX")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Steuerung für LED-Matrizen, Cubes und Pixelinstallationen")
set(CPACK_PACKAGE_DESCRIPTION
    "WhiteHoleFX berechnet Effekte als Shader auf der GPU und sendet sie per Art-Net, sACN oder DDP.\n Mit Oberfläche (Node-Editor, Layout-Editor, Regler) und Konsolen-Runner.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/bsteinmann/WhiteHoleFX")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
# Qt und die C++-Laufzeit ergibt dpkg-shlibdeps; libvulkan wird erst zur Laufzeit geladen
# (dlopen) und muss deshalb von Hand genannt werden.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "/usr/lib/whiteholefx")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libvulkan1")
# libasound2/libpulse0: miniaudio lädt sie erst zur Laufzeit; ohne sie gibt es keinen Audio-Eingang.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "mesa-vulkan-drivers, qt6-qpa-plugins, fonts-dejavu-core, libpulse0, libasound2t64 | libasound2")

include(CPack)
