cmake_minimum_required(VERSION 3.20)
project(cpp-ui-toolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# nanosvg (header-only SVG rasterizer)
FetchContent_Declare(
  nanosvg
  GIT_REPOSITORY https://github.com/memononen/nanosvg.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(nanosvg)

# Skia (prebuilt). Set SKIA_ROOT to a directory containing include/ and lib/.
set(SKIA_ROOT "" CACHE PATH "Path to prebuilt Skia")
if(SKIA_ROOT AND EXISTS "${SKIA_ROOT}/include/core/SkCanvas.h")
    set(SKIA_AVAILABLE 1)
    add_library(skia_prebuilt INTERFACE)
    target_include_directories(skia_prebuilt INTERFACE "${SKIA_ROOT}/include")
    target_link_directories(skia_prebuilt INTERFACE "${SKIA_ROOT}/lib")
    target_link_libraries(skia_prebuilt INTERFACE skia)
    add_compile_definitions(SKIA_AVAILABLE=1)
endif()

if(WIN32)
    add_executable(m0_win WIN32 src/platform/windows/m0_win.cpp)
    target_link_libraries(m0_win PRIVATE user32 gdi32 dwmapi msimg32 gdiplus)
    if(TARGET nanosvg)
        target_include_directories(m0_win PRIVATE ${nanosvg_SOURCE_DIR}/src)
    endif()
elseif(APPLE)
    add_executable(m0_mac src/platform/macos/m0_mac.mm)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(QUARTZCORE_LIBRARY QuartzCore)
    target_link_libraries(m0_mac PRIVATE ${COCOA_LIBRARY} ${QUARTZCORE_LIBRARY})
else()
    add_executable(m0_linux_x11 src/platform/linux/m0_linux_x11.cpp)
    find_package(X11 REQUIRED)
    target_link_libraries(m0_linux_x11 PRIVATE X11::X11)
    # Wayland (optional, requires wayland-client + generated xdg-shell protocol)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND QUIET wayland-client)
        if(WAYLAND_FOUND)
            add_executable(m0_linux_wayland src/platform/linux/m0_linux_wayland.cpp)
            target_include_directories(m0_linux_wayland PRIVATE ${WAYLAND_INCLUDE_DIRS})
            target_link_libraries(m0_linux_wayland PRIVATE ${WAYLAND_LIBRARIES})
        endif()
    endif()
endif()
