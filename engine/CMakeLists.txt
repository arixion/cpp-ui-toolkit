# Toad engine runtime. Must not depend on uitk: games ship without the editor's UI toolkit.
add_library(toad_engine STATIC src/engine.cpp)
add_library(toad::engine ALIAS toad_engine)
target_include_directories(toad_engine PUBLIC include)
uitk_warnings(toad_engine)
