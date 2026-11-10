# Toad: a 2D game engine and its editor, plus the uitk UI toolkit the editor is built on.
#   uitk/     UI toolkit (also builds on its own: cmake -S uitk)
#   engine/   runtime library (toad::engine)
#   editor/   the editor application (uitk + engine)
# Skia (used by uitk) is shared at third_party/skia; build it with tools/build_skia.sh <cpu|gpu>.
cmake_minimum_required(VERSION 3.24)
project(toad LANGUAGES C CXX)

# Settings every subproject must agree on (directory-scoped, so they are set here, above them).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")   # Skia is built against the release /MD CRT
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /EHsc /bigobj)
endif()
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()

option(UITK_RENDER_GPU "Build the GPU backend (uitk_render_gpu)" ON)   # the editor wants the GPU
option(TOAD_BUILD_EDITOR "Build the editor" ON)

enable_testing()
add_subdirectory(uitk)
add_subdirectory(engine)
if(TOAD_BUILD_EDITOR)
    add_subdirectory(editor)
endif()
