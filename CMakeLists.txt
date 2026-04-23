# Dependencies (download before building):
#   deps/nexus/Nexus.h  <- https://github.com/RaidcoreGG/RCGG-lib-nexus-api
#   deps/imgui/imgui.h  <- https://github.com/RaidcoreGG/imgui-v1.80
#
# Build:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
#   -> build/waitingame.dll  (drop into GW2/addons/waitingame/)

cmake_minimum_required(VERSION 3.20)
project(waitingame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(waitingame SHARED
    src/addon.cpp
    src/events.cpp
    src/game.cpp
    src/render.cpp
    deps/nexus/imgui/imgui.cpp
    deps/nexus/imgui/imgui_draw.cpp
    deps/nexus/imgui/imgui_widgets.cpp
    deps/nexus/imgui/imgui_tables.cpp
)

target_include_directories(waitingame PRIVATE
    src
    deps          # resolves "nexus/Nexus.h" -> deps/nexus/Nexus.h
    deps/nexus    # resolves "imgui/imgui.h" -> deps/nexus/imgui/imgui.h
)

target_compile_definitions(waitingame PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_link_options(waitingame PRIVATE
    -static-libgcc
    -static-libstdc++
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic
)

set_target_properties(waitingame PROPERTIES
    PREFIX ""
    OUTPUT_NAME "waitingame"
    SUFFIX ".dll"
)
